<p align="center">
  <img src="assets_github/banner.gif" alt="Banner">
</p>

<h1 align="center">WiFiKeyViewer</h1>
<p align="center">
  <b>A tiny Win32 utility to view the Wi-Fi passwords saved on your own PC.</b>
</p>

---

## About

**WiFiKeyViewer** lists the Wi-Fi profiles that Windows has already saved on the
current machine and shows their stored passwords (key content). It does this with
the standard, built-in Windows command:

```powershell
netsh wlan show profile name="<SSID>" key=clear
```

It also lets you export the list to a timestamped `wifi_report.txt`.

> **Scope / ethics.** This tool only reads credentials that are *already stored
> locally* by the current user on the device it runs on. It does **not** scan,
> attack, crack, or connect to any network, and it cannot read passwords from any
> other machine. Use it only on a device you own or are authorized to administer.

---

## Features

- View SSIDs and their saved passwords for the current PC.
- Export results to `wifi_report.txt` (with a timestamp).
- Simple owner-drawn Win32 UI (neon-on-black), rotating banner images.
- Optional local background music.
- Actions are also appended to `logs.log`.

---

## Build & Run (Windows, MinGW)

```powershell
g++ wifikeyviewer.cpp -o WiFiKeyViewer.exe -mwindows -std=c++17 ^
    -lgdiplus -lwinmm -lole32 -luuid -lcomctl32
```

With a compiled icon object (optional):

```powershell
windres icon.rc -o icon.o
g++ wifikeyviewer.cpp icon.o -o WiFiKeyViewer.exe -mwindows -std=c++17 ^
    -lgdiplus -lwinmm -lole32 -luuid -lcomctl32
```

Then run `WiFiKeyViewer.exe`. Reading `key=clear` may require running as
Administrator, depending on your Windows configuration.

---

## Project structure

```
WiFiKeyViewer/
├─ assets_github/     # banner + screenshots for this README
├─ images/            # rotating UI images
├─ music/             # optional background tracks
├─ icon.rc            # icon resource script
├─ wifikeyviewer.cpp  # source
├─ LICENSE
└─ README.md
```

---

## Credits & License

This is an adaptation by Yeldana Yeshmuratova.

Based on an original MIT-licensed project by **MR.ShadowMan** (2025). The original
theming and Win32 scaffolding are reused under the MIT License; the credential
handling was rewritten, the misleading "hacking" framing removed, and an export
feature added. See [LICENSE](LICENSE) for the full terms and both copyright lines.
