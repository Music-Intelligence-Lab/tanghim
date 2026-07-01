---
title: Program Change Preset Switching
description: Switch presets using MIDI Program Change messages.
---

Tanghim responds to MIDI Program Change messages for preset switching:

- **PC 0–7** activates presets 1–8 respectively.
- Program Change messages from any source (DAW MIDI clips, external controllers, etc.) are accepted.
- This is always on — no configuration needed.
- Program Change messages are consumed and not passed through to the output.

This works alongside [MIDI Preset Mapping](/docs/performance/midi-preset-mapping), which uses a dedicated MIDI input device for note-based switching. The two methods are independent and can be used simultaneously.
