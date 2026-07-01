---
title: Introduction
description: What Tanghīm is, the two plugins, and the DiArMaqAr data source.
---

Tanghīm is an open-source plugin that accesses Arabic maqām tuning data from [DiArMaqAr](https://diarmaqar.net) — the Digital Arabic Maqām Archive — and applies it to your software and hardware instruments across any DAW. Tuning data is stored in a local cache; internet access is only required the first time you select a particular tuning system.

**The name:** Tanghīm (تنغيم) is the singular of tanāghīm — the tuning-systems layer of the DiArMaqAr taxonomy — meaning both a tuning system and the act of tuning.

## Two Plugins

The project ships as two complementary plugins:

- **Tanghim (Transmitter)** — the main plugin with a full graphical interface. It broadcasts tuning via MTS-ESP and hosts a built-in reference oscillator. Load it on any instrument track in your DAW.
- **Tanghim Receiver** — a lightweight MIDI effect that reads the MTS-ESP tuning broadcast and converts it to MPE per-note pitch bend or monophonic 14-bit pitch bend, for instruments that don't support MTS-ESP natively.

For Ableton Live, where VST3 MIDI effects are not supported, two Max for Live devices are provided instead:

- **Tanghim MPE Receiver** — an M4L MIDI effect for MPE-enabled instruments
- **Tanghim Mono PB Receiver** — an M4L MIDI effect for instruments that only respond to standard pitch bend

See [Ableton Live](/docs/ableton) for setup details.

## Formats and Platforms

| Platform | Formats |
|---|---|
| macOS | VST3, AU, CLAP |
| Windows | VST3, CLAP |
| Linux | VST3, CLAP |

Tanghīm was conceived and designed by [Khyam Allami](https://khyamallami.com) as part of postdoctoral research at the [Music Intelligence Lab](https://musicintelligencelab.com/), American University of Beirut, 2026.
