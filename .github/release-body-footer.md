<!-- Appended to every GitHub Release body after the CHANGELOG.md section.
     __VERSION__ is substituted with the release version by release.yml.
     The changelog itself lives in CHANGELOG.md - see RELEASING.md. -->

## Downloads

| Platform | Installer |
|---|---|
| macOS (Universal) | Tanghim-__VERSION__-macOS.pkg |
| Windows (x64) | Tanghim-__VERSION__-Windows.exe |
| Linux (x64) | Tanghim-__VERSION__-Linux.tar.gz |

### Installation

- **macOS:** Double-click the `.pkg`. On first plug-in launch you may need to right-click the plug-in file in Finder → Open to bypass Gatekeeper (installer is ad-hoc signed, not notarized).
- **Windows:** Double-click the `.exe`. Admin rights required (installs to `Program Files`).
- **Linux:** `tar xzf Tanghim-__VERSION__-Linux.tar.gz && cd Tanghim-__VERSION__-Linux && ./install.sh`

Each install first clears the previous version's files (and any left in wrong locations) so you always run the latest, correctly-placed build.

### Uninstalling

Full removal, including cached data, settings and presets:

- **macOS:** `sudo bash uninstall-macos.sh`
- **Windows:** right-click `uninstall-windows.bat` → **Run as administrator**

### What's included

- Tanghim (Transmitter) — VST3, AU (macOS only), CLAP
- Tanghim Receiver — VST3, AU (macOS only), CLAP
- Tanghim M4L Receivers (MPE + Mono PB) for Ableton Live — macOS and Windows installers, optional component

> **Ableton Live users:** Ableton doesn't support VST3 MIDI effects. Enable the M4L component during installation, or use MTS-ESP-capable synths.
