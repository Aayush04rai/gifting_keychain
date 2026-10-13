"""Tests the SECURE find-my-keychain path from a Windows PC: real passkey pairing.

The keychain shows a 6-digit passkey on its screen; here the test build also prints
it to the serial log (FIND_LOG_PASSKEY=1) so this script can type it in, the way a
person would on a phone. Afterwards it rings the keychain over the encrypted link.

    python tools/find_test_secure.py [COM10] [level 1|2] [seconds]

Needs `pip install bleak pyserial` (bleak brings the WinRT bindings).
"""
import asyncio
import queue
import re
import sys
import threading
import time

import serial
from bleak import BleakClient, BleakScanner
from winrt.windows.devices.bluetooth import BluetoothLEDevice
from winrt.windows.devices.enumeration import DevicePairingKinds, DevicePairingProtectionLevel, DevicePairingResultStatus

IAS_ALERT_LEVEL = "00002a06-0000-1000-8000-00805f9b34fb"
PORT = sys.argv[1] if len(sys.argv) > 1 else "COM10"
LEVEL = int(sys.argv[2]) if len(sys.argv) > 2 else 2      # 1 = gentle, 2 = strong
SECS = float(sys.argv[3]) if len(sys.argv) > 3 else 5.0  # how long to ring

passkeys: "queue.Queue[str]" = queue.Queue()
log_lines: list[str] = []
stop_serial = False


def serial_reader():
    s = serial.Serial(PORT, 115200, timeout=0.2)
    s.dtr = False
    s.rts = False
    buf = b""
    while not stop_serial:
        buf += s.read(512)
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            text = line.decode(errors="replace").strip()
            if "ble_find" in text:
                log_lines.append(text)
                print("  [device]", text)
            m = re.search(r"TESTPASSKEY (\d{6})", text)
            if m:
                passkeys.put(m.group(1))


async def main():
    threading.Thread(target=serial_reader, daemon=True).start()
    time.sleep(1.0)

    # Opening the serial port resets the board; give it time to boot and start
    # advertising, then scan a few times (a scan can miss a 1-1.5 s beacon).
    time.sleep(5.0)
    keys = []
    for attempt in range(4):
        print("scanning...", attempt + 1)
        found = await BleakScanner.discover(timeout=8.0, return_adv=True)
        keys = [(d, a) for d, a in found.values() if (d.name or a.local_name or "").startswith("Keychain")]
        if keys:
            break
    if not keys:
        print("no keychain found")
        return
    dev = keys[0][0]
    print("found", dev.name, dev.address)

    le = await BluetoothLEDevice.from_bluetooth_address_async(int(dev.address.replace(":", ""), 16))
    custom = le.device_information.pairing.custom

    def on_pairing_requested(sender, args):
        print("  pairing kind requested:", args.pairing_kind)
        if args.pairing_kind & DevicePairingKinds.PROVIDE_PIN:
            deferral = args.get_deferral()

            def wait_and_accept():
                try:
                    pin = passkeys.get(timeout=20)
                    print("  entering passkey", pin)
                    args.accept_with_pin(pin)
                except queue.Empty:
                    print("  no passkey shown by the device!")
                finally:
                    deferral.complete()

            threading.Thread(target=wait_and_accept, daemon=True).start()
        else:
            args.accept()

    token = custom.add_pairing_requested(on_pairing_requested)
    if le.device_information.pairing.is_paired:
        print("already paired")
    else:
        print("pairing (the passkey appears on the keychain)...")
        result = await custom.pair_with_protection_level_async(
            DevicePairingKinds.PROVIDE_PIN, DevicePairingProtectionLevel.ENCRYPTION_AND_AUTHENTICATION
        )
        print("pairing result:", DevicePairingResultStatus(result.status).name)
    custom.remove_pairing_requested(token)

    print("connecting and ringing over the encrypted link...")
    async with BleakClient(dev) as client:
        await client.write_gatt_char(IAS_ALERT_LEVEL, bytes([LEVEL]), response=False)
        await asyncio.sleep(SECS)
        await client.write_gatt_char(IAS_ALERT_LEVEL, bytes([0]), response=False)
        await asyncio.sleep(1.5)
    print("done")
    global stop_serial
    stop_serial = True


asyncio.run(main())
