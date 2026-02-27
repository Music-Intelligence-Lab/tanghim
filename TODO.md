## Features


## Bugs
- Preset Midi mapping input device works perfect on first fresh load in a blank DAW session, but if I remove the plugin and reload it, it doesn't work. It's like the device selection needs to be refreshed but not only in UI. I don't know.


## Documentation and Code Symbol updates
- add a note about the replaceCharacter('_', ':') issue/bug to our claude.md so it doesn't happen again

- I need you to document your research about enabeling "tuning" in Max and M4L devices in our diary. This will be useful for later so we can create a guide for how to do it and save ourselves this work in the future.

- Update all references to Arabic Maqam Tuner including C++ class names and any styling etc... we should have none, everything should be Tanghīm/Tanghim. And change our project directory name to "tanghim".


## Performance

- slot/slider thumb value automation not refreshing fast enough in the transmitter VST UI., is thist related to: slot_N automation still calls `updateTuningAndBroadcast()` (full 128 `std::pow` rebuild) on every MIDI CC tick — could use a fast path similar to ref_freq

- CriticalSection on tuning tables → consider double-buffered atomic swap (see Session 38 audit notes)


## Paths
/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client MPE.amxd

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client.amxd