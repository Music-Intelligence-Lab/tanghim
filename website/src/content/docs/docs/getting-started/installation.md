---
title: Installation
description: Download, install, and verify Tanghīm on macOS, Windows, and Linux.
---

Download the latest release for your platform from the [GitHub Releases page](https://github.com/Music-Intelligence-Lab/tanghim/releases).

For Ableton Live, see the dedicated [Ableton Live setup guide](/docs/ableton).

## What Gets Installed

After installation you will have two plugins:

- **Tanghim** (Transmitter): `.vst3` / `.component` / `.clap`
- **Tanghim Receiver**: `.vst3` / `.component` / `.clap`

If they did not auto-install, copy the plugin files to the appropriate system directories below, then rescan plugins in your DAW.

## macOS

| Format | Install Path |
|--------|-------------|
| VST3 | `~/Library/Audio/Plug-Ins/VST3/` |
| AU | `~/Library/Audio/Plug-Ins/Components/` |
| CLAP | `~/Library/Audio/Plug-Ins/CLAP/` |

On first launch, macOS may block the plugin. Right-click the `.vst3` bundle → **Open** to bypass Gatekeeper, or go to **System Settings → Privacy & Security** and click **Allow**.

## Windows

| Format | Install Path |
|--------|-------------|
| VST3 | `C:\Program Files\Common Files\VST3\` |
| CLAP | `C:\Program Files\Common Files\CLAP\` |

AU is not available on Windows.

## Linux

| Format | Install Path |
|--------|-------------|
| VST3 | `~/.vst3/` |
| CLAP | `~/.clap/` |

AU is not available on Linux. Some DAWs also check `/usr/lib/vst3/` and `/usr/lib/clap/`; consult your DAW's documentation if the plugin isn't detected.

## MTS-ESP Shared Library (Required)

Tanghīm broadcasts tuning through ODDSound's MTS-ESP system, which relies on a shared system library (`libMTS`) that all MTS-ESP software loads at runtime. **The installers install it for you**; you only need this section if you installed the plugins manually, or if tuning isn't reaching your synth.

**Symptom of a missing library:** the MTS-ESP badge lights up in Tanghīm (it looks connected), but no synth is actually retuned: MTS-ESP-native synths (e.g. Surge XT) don't appear in the badge count, and the MPE / Mono PB receivers stay at standard 12-tone tuning.

If this happens, install the library to the correct system location:

| Platform | Library path |
|---|---|
| macOS | `/Library/Application Support/MTS-ESP/libMTS.dylib` |
| Windows | `C:\Program Files\Common Files\MTS-ESP\LIBMTS.dll` |
| Linux | `/usr/local/lib/libMTS.so` |

The library file is bundled with each release. It is shared across all MTS-ESP products on your machine; installers only add it if it is not already present and never remove it on uninstall.

## Data & Preferences

Tanghīm stores cached tuning data, settings, and exported MIDI files locally:

| | macOS | Windows | Linux |
|---|---|---|---|
| Settings | `~/Library/Tanghim/settings.json` | `%APPDATA%\Tanghim\settings.json` | `~/.config/Tanghim/settings.json` |
| Cache | `~/Library/Tanghim/cache/` | `%APPDATA%\Tanghim\cache\` | `~/.config/Tanghim/cache/` |
| MIDI export | `~/Library/Tanghim/midi-export/` | `%APPDATA%\Tanghim\midi-export\` | `~/.config/Tanghim/midi-export/` |

## Uninstalling

Each release ships a standalone uninstaller that fully removes Tanghīm: both plugins, the Max for Live devices and Max package (if installed), and all local data (cached tuning data, settings, presets, exported MIDI).

| Platform | How to uninstall |
|---|---|
| macOS | Download `uninstall-macos.sh` from the release, then run `sudo bash uninstall-macos.sh` |
| Windows | Download `uninstall-windows.bat` from the release, right-click → **Run as administrator** |
| Linux | Delete the plugin files from `~/.vst3/` and `~/.clap/`, and the data folder `~/.config/Tanghim/` |

Reinstalling does not require uninstalling first. Every installer automatically clears the previous version's files before installing the new ones, while preserving your settings, presets, and cache. Use the uninstaller only for a complete removal or a fully clean reset.
