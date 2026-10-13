"""Pushes assets/sd/ to the microSD in the device, over its USB cable.

    <idf python> tools/sd_push.py [COM10] [folder]

Needs pyserial (the ESP-IDF python env has it). Files go to the same relative
path on the card. The firmware must be the version with sd_upload.c.
"""
import os, sys, time
import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM10"
root = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "sd")

s = serial.Serial(port, 115200, timeout=0.2)
s.dtr = False; s.rts = False  # do not reset the chip on open
# Opening the port can reset the board on some hosts; give it time to boot
# and start the listener (about 1.5 s) before sending anything.
time.sleep(3.5)
s.reset_input_buffer()

def wait_reply(path, timeout=15):
    end, buf = time.time() + timeout, b""
    while time.time() < end:
        buf += s.read(256)
        for line in buf.split(b"\n"):
            if line.startswith(b"SDOK ") and line[5:].strip().decode() == path:
                return True
            if line.startswith(b"SDERR"):
                print("  device:", line.decode(errors="replace").strip()); return False
    print("  no reply"); return False

fails = 0
for d, _, names in os.walk(root):
    for n in sorted(names):
        full = os.path.join(d, n)
        rel = os.path.relpath(full, root).replace("\\", "/")
        data = open(full, "rb").read()
        print(f"{rel} ({len(data)} bytes)")
        s.write(b"\nSDPUT %s %d\n" % (rel.encode(), len(data)))
        sent_ok = True
        seen = b""
        for i in range(0, len(data), 1024):
            s.write(data[i:i + 1024])
            s.flush()
            end = time.time() + 5
            while True:  # wait for the device's ACK (0x06) for this block
                b = s.read(64)
                seen += b
                if b"" in b:
                    break
                if b"" in b or b"SDERR" in b or time.time() > end:
                    print("  block lost:", b[:60]); sent_ok = False; break
            if not sent_ok:
                break
        if not sent_ok:
            fails += 1; s.reset_input_buffer(); time.sleep(4); continue
        # Every block was ACKed after being written to the card.
        print("  ok")

print("done" if not fails else f"{fails} file(s) failed")
sys.exit(1 if fails else 0)
