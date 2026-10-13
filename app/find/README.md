# Find my keychain - phone page

One page (`index.html`) that talks to the keychain over Bluetooth LE: connect, ring,
stop, battery and a rough "how close" meter. It uses only the standard Immediate
Alert / Battery services the keychain exposes (see `main/board/ble_find.h`).

## Running it

Web Bluetooth only works on a **secure origin** (HTTPS, or `http://localhost`).

- **Quick test on a computer:** `python -m http.server 8000` in this folder, then open
  `http://localhost:8000` in Chrome or Edge.
- **For phones:** put the folder on any HTTPS static host (GitHub Pages, Netlify, an
  internal web server). Nothing is sent to the host beyond the page itself.

## Which phones

| Phone | How |
|---|---|
| Android | Chrome. Works as is. "Add to Home screen" gives it an app-like icon. |
| iPhone | Safari does **not** support Web Bluetooth. Open the page in the free **Bluefy** browser (fine for a demo), or ship a native app. |

All Bluetooth calls go through the small `ble` object near the top of `index.html`
(`pick`, `connect`, `setAlert`, `readBattery`). A native wrapper such as Capacitor
with its Bluetooth LE plugin would implement those same four calls and the rest of
the page would stay unchanged - that is the route to a real iOS app (needs a Mac,
Xcode and an Apple developer account).

## Pairing

The first connection pairs: the keychain shows a 6-digit code on its screen and the
phone asks for it. After that the phone is remembered and reconnects with no code.
New pairings are accepted for the first 10 minutes after the keychain powers up (and
whenever it has never been paired).
