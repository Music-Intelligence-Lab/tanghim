---
title: MIDI Preset Mapping
description: Map MIDI notes to presets for live maqam switching.
---

You can map MIDI notes to presets so you can switch between maqamat during performance using a MIDI controller.

## Setup

1. In the status bar, select your **MIDI input device** and **channel** from the dropdowns.
2. **Shift+click** a preset slot to enter learn mode.
3. Play the desired MIDI note on your controller.
4. The note is now mapped — a badge appears on the preset showing the assigned note.

## Clearing a Mapping

Click the note badge on a preset to remove the mapping.

## Important Notes

- MIDI preset mapping uses a **dedicated MIDI input** separate from your DAW's MIDI routing, ensuring reliable triggering regardless of DAW state.
- MIDI notes on the preset mapping channel are filtered from the oscillator to prevent double-triggering.
- Device and mapping settings are saved in `~/Library/Tanghim/settings.json` (macOS) or the equivalent location on Windows/Linux.

## Ableton Live Users

Ableton merges all incoming MIDI to channel 1 between tracks. If your MIDI controller sends preset pads on a different channel (e.g. channel 10), set Ableton's MIDI track input to a specific channel (e.g. channel 1 for keys) rather than "All Channels". This way, pad notes on channel 10 won't be routed to the plugin and won't accidentally trigger the oscillator. The dedicated MIDI input still receives all channels directly, so preset switching works regardless of this setting.
