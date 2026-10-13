"""Downloads the project's fonts from the Google Fonts repository and builds the
small static Latin TTFs (Regular and SemiBold) used on the device.

    python -I utility/fonts/fetch_fonts.py

Needs `pip install fonttools`. Downloads go to a temporary folder (they are
treated as untrusted data: nothing from them is ever imported or run); the
finished fonts are written next to this script. Everything is under the SIL Open
Font License; the licence texts are saved in licenses/.
"""
import os
import sys
import tempfile
import urllib.parse
import urllib.request

from fontTools import subset
from fontTools.ttLib import TTFont
from fontTools.varLib import instancer

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = "https://raw.githubusercontent.com/google/fonts/main/ofl/"

# family folder -> (output name, file in the repo, extra axes to pin)  [variable fonts]
VARIABLE = {
    "inter": ("Inter", "Inter[opsz,wght].ttf", {"opsz": 14}),
    "ibmplexsans": ("IBMPlexSans", "IBMPlexSans[wdth,wght].ttf", {"wdth": 100}),
    "manrope": ("Manrope", "Manrope[wght].ttf", {}),
    "dmsans": ("DMSans", "DMSans[opsz,wght].ttf", {"opsz": 14}),
    "sora": ("Sora", "Sora[wght].ttf", {}),
    "montserrat": ("Montserrat", "Montserrat[wght].ttf", {}),
}
# family folder -> (output name, {weight: file})  [already-static fonts]
STATIC = {"poppins": ("Poppins", {400: "Poppins-Regular.ttf", 600: "Poppins-SemiBold.ttf"})}

# Basic Latin, Latin-1, and the typographic punctuation headlines use.
UNICODES = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100)) + [
    0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC, 0x20B9, 0x2122, 0x00B0,
]
NAMES = {400: "Regular", 600: "SemiBold"}


def fetch(folder, filename, dest):
    url = BASE + folder + "/" + urllib.parse.quote(filename)
    urllib.request.urlretrieve(url, dest)


def slim(path_in, path_out):
    opts = subset.Options()
    opts.layout_features = ["kern", "liga"]
    opts.name_IDs = [0, 1, 2, 3, 4, 5, 6, 13, 14]
    opts.notdef_outline = True
    opts.hinting = False
    opts.desubroutinize = True
    font = TTFont(path_in)
    sub = subset.Subsetter(opts)
    sub.populate(unicodes=UNICODES)
    sub.subset(font)
    font.save(path_out)


def main():
    os.makedirs(os.path.join(HERE, "licenses"), exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for folder, (name, repo_file, axes) in VARIABLE.items():
            raw = os.path.join(tmp, name + "-var.ttf")
            fetch(folder, repo_file, raw)
            for w in (400, 600):
                inst = instancer.instantiateVariableFont(TTFont(raw), {"wght": w, **axes})
                mid = os.path.join(tmp, f"{name}-{w}.ttf")
                inst.save(mid)
                dst = os.path.join(HERE, f"{name}-{NAMES[w]}.ttf")
                slim(mid, dst)
                print(dst, os.path.getsize(dst))
        for folder, (name, files) in STATIC.items():
            for w, repo_file in files.items():
                raw = os.path.join(tmp, repo_file)
                fetch(folder, repo_file, raw)
                dst = os.path.join(HERE, f"{name}-{NAMES[w]}.ttf")
                slim(raw, dst)
                print(dst, os.path.getsize(dst))
        for folder in list(VARIABLE) + list(STATIC):
            try:
                fetch(folder, "OFL.txt", os.path.join(HERE, "licenses", folder + "-OFL.txt"))
            except OSError as e:
                print("no licence file for", folder, e, file=sys.stderr)


if __name__ == "__main__":
    main()
