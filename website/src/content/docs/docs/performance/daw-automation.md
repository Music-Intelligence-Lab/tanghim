---
title: DAW Automation
description: Automatable parameters and MIDI CC mapping.
---

Tanghīm exposes its key tuning parameters to your DAW's automation system. You can draw automation curves, map MIDI CC, or use any DAW automation feature to control these parameters in real time.

## Automatable Parameters

| Parameter | Range | Description |
|---|---|---|
| `Slider 1` – `Slider 12` | ±150 cents | Cents deviation for each of the 12 chromatic pitch classes. Slider 1 = C, Slider 2 = C#, etc. |
| `Ref Freq` | ±700 cents | Reference frequency offset from the tuning system's default. |
| `Preset` | None, 1–8 | Active preset index. Automate this to switch between saved maqam presets at specific points in your arrangement. |

## Usage Tips

- **Automating sliders** (`Slider 1`–`Slider 12`) lets you smoothly glide tuning between positions — useful for creative effects or gradual intonation shifts during a performance.
- **Automating the preset parameter** is a straightforward way to switch between maqamat at defined points in a song without MIDI preset mapping.
- **Automating the reference frequency** lets you create pitch drifts or transpose the entire tuning smoothly over time.
- All automation updates are applied at audio-rate for glitch-free transitions.
- Automation and manual slider adjustments coexist — the last value written (whether from automation or the UI) takes effect.

## MIDI CC Mapping

Most DAWs allow you to map MIDI CC messages to plugin parameters. You can control any of the above parameters from a hardware MIDI controller's knobs or faders. Consult your DAW's documentation for how to set up MIDI CC → parameter mapping (sometimes called "MIDI Learn" on the DAW side — this is separate from Tanghīm's own [MIDI Preset Mapping](/docs/performance/midi-preset-mapping) feature).
