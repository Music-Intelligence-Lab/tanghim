## Features

- still need to fix the automation not refreshing fast enough in the transmitter VST UI

- M4L receiver Midi notes have timing issues, likely to do with the Metro. 
- M4L receiver MPE mode active channels monitoring doesn't seem to be working.
- ~~M4L receiver Mono PB mode: pitch bend wheel override bug + PB range minimum~~ **DONE** (Session 32 — user PB now combines with microtuning offset, PB range min set to 2)

## Documentation and Code Symbol updates

- I need you to document your research about enabeling "tuning" in Max and M4L devices in our diary. This will be useful for later so we can create a guide for how to do it and save ourselves this work in the future.

- Update all references to Arabic Maqam Tuner including C++ class names and any styling etc... we should have none, everything should be Tanghīm/Tanghim. And change our project directory name to "tanghim".


## Performance

- ~~ref_freq MIDI CC CPU spike: parameterChanged used full table rebuild + JSON serialize at MIDI CC rate~~ **DONE** (Session 41 — fast path + dirty flag throttle)
- ~~Oscillator double precision + per-sample branching~~ **DONE** (Session 41 — float + segmented block rendering)
- slot_N automation still calls `updateTuningAndBroadcast()` (full 128 `std::pow` rebuild) on every MIDI CC tick — could use a fast path similar to ref_freq
- CriticalSection on tuning tables → consider double-buffered atomic swap (see Session 38 audit notes)


## Paths
/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client MPE.amxd

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client.amxd