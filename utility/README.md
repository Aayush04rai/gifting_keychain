# utility

Everything that supports the project but is not part of the firmware itself.

| Folder | What it is |
|---|---|
| `tools/` | Helper scripts for the device: generate the card artwork (`make_art.py`), convert it to LVGL images (`build_assets.py`), copy files to the SD card over USB (`sd_push.py`), and test find-my-keychain from a PC (`find_test.py`, `find_test_secure.py`). |
| `fonts/` | The typefaces under consideration for the UI (Inter, IBM Plex Sans, Manrope, DM Sans, Sora, Poppins, with Montserrat as the current baseline), each as small static Latin Regular and SemiBold TTFs, plus their licences and a comparison image (`preview.png`). |

Run the scripts from the project root, for example:

```
python utility/tools/make_art.py                      # system Python with Pillow
<idf python> utility/tools/build_assets.py            # the ESP-IDF Python
<idf python> utility/tools/sd_push.py COM10           # needs the device connected
python -I utility/fonts/fetch_fonts.py                # re-download and rebuild the fonts (needs fonttools)
```

## Fonts

All fonts are under the SIL Open Font License (free for commercial use; keep the licence
text with them, see `fonts/licenses/`). They were downloaded from the Google Fonts
repository by `fetch_fonts.py`, which pins the weights, trims each file to Latin letters,
numbers and common punctuation (about 15-35 KB each), and treats the downloads as data
only: nothing from them is ever imported or run.

Once a family is chosen, copy its two files to the SD card (`assets/sd/fonts/`) so the
device can load them at start-up.

## Other folders

- `../app/find/` is the phone page for find-my-keychain.
- `../archive/` holds code that is parked, not built: the earlier watch faces and the
  cricket feature.
