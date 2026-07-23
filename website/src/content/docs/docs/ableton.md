---
title: Using Tanghim in Ableton Live
description: Max for Live devices and Ableton-specific setup for maqam tuning delivery.
---

Ableton Live has specific requirements that affect how Tanghīm works. This page covers everything you need to know.

## Why Ableton Needs Special Handling

Ableton Live **does not support VST3 MIDI effects**. This means the Tanghīm Receiver plugin cannot be loaded directly in Ableton as it can in other DAWs. Additionally, Ableton **merges all MIDI to channel 1** between tracks, which breaks MPE routing between tracks.

To work around these limitations, Tanghīm provides two **Max for Live Receiver** devices that replicate the Receiver's MPE and Mono Pitch Bend functionality within Ableton's MIDI effect framework.

## Setup with MTS-ESP

If your instrument supports MTS-ESP, this is the simplest path: no Receiver or M4L device needed.

1. Create a new **MIDI track**.
2. Load **Tanghīm** as an instrument on that track.
3. On any other instrument track, load an MTS-ESP-compatible synth (e.g. Surge XT, Vital).
4. The synth will automatically receive the tuning broadcast.

## Setup with Max for Live Receivers

For instruments that don't support MTS-ESP (including hardware synths and many software instruments), Tanghīm ships two native Max for Live devices:

- **Tanghim MPE Receiver.amxd**: per-note channel allocation with per-note pitch bend (MPE)
- **Tanghim Mono PB Receiver.amxd**: single-channel monophonic 14-bit pitch bend

### MPE Compatibility

As of Live 12, all Live instruments support MPE. In Live 11, MPE-compatible devices include: Drift, Wavetable, Sampler, Simpler (convert to Sampler to change MPE settings), Arpeggiator, and AAS devices (Analog, Tension, Collision, Electric).

### Installing the Max for Live Devices

The macOS and Windows installers install both devices and the required Max package automatically when the M4L component is enabled. If you are building from source:

1. Copy both `.amxd` files from `m4l/` into:
   ```
   ~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim/
   ```
2. Copy the entire `m4l/MTS-ESP-Max-Package/` directory into both:
   ```
   ~/Documents/Max 8/Packages/MTS-ESP-Max-Package/   (Live 11)
   ~/Documents/Max 9/Packages/MTS-ESP-Max-Package/   (Live 12)
   ```
3. Restart Ableton Live.

The Max package contains the ODDSound MTS-ESP externals (the `MTS-ESP.mtof` Max object the receiver patches use). It must live in the Max **Packages** folder (not `Library/`) so Max registers it as a structured package.

### Using a Max for Live Receiver

1. Load the main **Tanghīm** plugin on one MIDI track.
2. On your instrument track, add either **Tanghim MPE Receiver** or **Tanghim Mono PB Receiver** as a MIDI effect *before* your instrument (see the [tuning method comparison](/docs/tuning/methods#which-method-should-i-use)).
3. Set the pitch bend range in the device to match your instrument's setting.
4. Play, and the device reads the MTS-ESP tuning and applies pitch bend to each note.

Both devices are pure-native Max patches. They read the live MTS-ESP tuning broadcast directly (no VST3 bridge, no JavaScript) and emit standard MPE or 14-bit mono pitch-bend MIDI.

## Ableton-Specific Notes

- **Computer MIDI Keyboard**: When the Tanghīm plugin window is focused, Ableton's built-in computer keyboard MIDI input is disabled (standard DAW behavior when a plugin window has focus). Click outside the plugin window to return focus to Ableton, or use an external MIDI controller.
- **Plugin scanning**: On Apple Silicon Macs, Ableton's plugin scanner runs under Rosetta (x86_64). The plugin is built as a universal binary to ensure compatibility.
- **MIDI routing between tracks**: Ableton normalizes MIDI to channel 1 between tracks. This is why the standalone Receiver VST3 cannot be used in Ableton: you must use the M4L Receiver device for MPE and Mono PB delivery, or use MTS-ESP directly.
- **MIDI preset mapping with multi-channel controllers**: If your MIDI controller sends keys on one channel and pads on another (e.g. keys on ch1, pads on ch10), set the Ableton track's MIDI input to the keys channel only, not "All Channels". Because Ableton merges all channels to ch1, the plugin's channel filter cannot distinguish pad notes from key notes if both arrive on the same channel. The dedicated MIDI preset input receives directly from the device and preserves channel info, so preset switching works correctly regardless.
