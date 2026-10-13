"""Converts tools/art_png/*.png to LVGL .bin (RGB565A8) in assets/sd/ui/.

Needs LVGLImage.py's dependencies (pypng, lz4), which the ESP-IDF python env
has. Run with that interpreter:

    <idf python> tools/build_assets.py

Then copy assets/sd/ to the root of the FAT32 microSD card.
"""
import glob, os, subprocess, sys

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
conv = os.path.join(root, "managed_components", "lvgl__lvgl", "scripts", "LVGLImage.py")
out = os.path.join(root, "assets", "sd", "ui")
os.makedirs(out, exist_ok=True)
for png in sorted(glob.glob(os.path.join(root, "tools", "art_png", "*.png"))):
    subprocess.check_call([sys.executable, conv, "--ofmt", "BIN", "--cf", ("RGB565" if os.path.basename(png).startswith("home") else "RGB565A8"), "--compress", "NONE",
                           "-o", out, png])
    print("built", os.path.basename(png))
