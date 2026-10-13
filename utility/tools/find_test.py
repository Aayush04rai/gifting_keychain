"""Talks to the keychain over Bluetooth from a PC, to test "find my keychain".

    python utility/tools/find_test.py scan            # list nearby keychains
    python utility/tools/find_test.py ring [seconds]  # connect, ring, stop after N s (default 6)

Needs `pip install bleak`. A build with FIND_INSECURE_TEST=1 accepts this without
pairing; the shipping build requires the passkey pairing (the OS shows a dialog).
"""
import asyncio
import sys

from bleak import BleakClient, BleakScanner

IAS_ALERT_LEVEL = "00002a06-0000-1000-8000-00805f9b34fb"
BATTERY_LEVEL = "00002a19-0000-1000-8000-00805f9b34fb"


async def find():
    print("scanning for 8 s...")
    found = await BleakScanner.discover(timeout=8.0, return_adv=True)
    keys = [(d, a) for d, a in found.values() if (d.name or a.local_name or "").startswith("Keychain")]
    for d, a in keys:
        print(f"  {d.name or a.local_name}  {d.address}  rssi {a.rssi} dBm  services {a.service_uuids}")
    return keys


async def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "scan"
    keys = await find()
    if cmd == "scan" or not keys:
        if not keys:
            print("no keychain found")
        return
    dev = keys[0][0]
    secs = float(sys.argv[2]) if len(sys.argv) > 2 else 6.0
    async with BleakClient(dev) as client:
        print("connected:", client.is_connected)
        try:
            print("battery:", (await client.read_gatt_char(BATTERY_LEVEL))[0], "%")
        except Exception as e:  # noqa: BLE001
            print("battery read failed:", e)
        print("ringing...")
        await client.write_gatt_char(IAS_ALERT_LEVEL, bytes([2]), response=False)
        await asyncio.sleep(secs)
        print("stopping...")
        await client.write_gatt_char(IAS_ALERT_LEVEL, bytes([0]), response=False)
        await asyncio.sleep(1.5)
    print("done")


asyncio.run(main())
