---
title: Troubleshooting
description: Common issues and how to resolve them.
---

## Plugin doesn't appear in DAW

- Verify the plugin files are in the correct directory for your OS (see [Installation](/docs/getting-started/installation)).
- Rescan plugins in your DAW's preferences.
- On macOS, you may need to bypass Gatekeeper (see the [macOS installation section](/docs/getting-started/installation#macos)).

## MTS-ESP not connecting

- Ensure only **one** Tanghīm transmitter instance is running. Multiple transmitters will conflict.
- Check that your instrument has MTS-ESP support enabled in its settings.
- Check that the MTS-ESP badge (teal) is lit in the top bar, confirming the transmitter is active.
- If the badge is lit but no synths are being retuned, the MTS-ESP shared library (`libMTS`) may not be installed. See [MTS-ESP Shared Library](/docs/getting-started/installation#mts-esp-shared-library-required).

## Plugin crashes in Ableton (SIGKILL / Code Signature Invalid)

If you are building from source: when reinstalling the plugin, you must **delete** the old `.vst3` bundle before copying the new one. Overwriting corrupts macOS's kernel code signature cache, causing Ableton's Rosetta plugin scanner to kill the plugin. Always:

```bash
rm -rf ~/Library/Audio/Plug-Ins/VST3/Tanghim.vst3
# then copy the new build
```

## No sound from internal oscillator

- Click the **Osc** badge (amber) in the top bar to make sure it is enabled.
- Check that MIDI is reaching the plugin (the slider bank shows gold thumb glows on active notes).
- The oscillator is quiet by design (−18 dBFS) — check your output volume.

## Stale tuning data

Click **Check for Updates** in the status bar — the plugin also checks automatically when it loads. If updates are available, the button turns **gold** and reads "Update Available"; click it to download the latest data. You can also click **Clear Cache** (which asks for confirmation) to wipe all cached tuning data and force a fresh fetch on next load. Your presets are preserved.

## MIDI preset device not responding

- Check that the correct MIDI input device and channel are selected in the status bar.
- If the device was disconnected and reconnected, the plugin should auto-detect it. If not, reselect it from the dropdown.

## Receiver shows "No transmitter found"

The Receiver establishes its connection inside the audio processing callback. Hosts that do not run the Receiver's audio thread will show this message even when a Transmitter is active. Two known cases:

- **Logic Pro AU**: Logic suspends a MIDI-effect AU track when no instrument is loaded downstream and the transport is stopped. Load a synth after the Receiver on the same track, or start playback, to activate the track.
- **Ableton Live**: Live does not run the VST3 Receiver's audio thread. Use the [Max for Live Receiver](/docs/ableton) instead.
