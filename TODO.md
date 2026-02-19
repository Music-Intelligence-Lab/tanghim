## Features

- Add an internal oscillator (triangle wave) with ADSR and allow computer keyboard input so user can play the tuner without needing MIDI input if desired. This should be activated by a button named "Oscillator" or "Osc" next to the MTS-ESP, MPE and Pitch Bend buttons. The oscillator should be **enableable alongside** the other output modes (not mutually exclusive) so the user can play both the internal oscillator and their synth simultaneously to verify the synth is producing the correct tuning

- Develop a **Receiver** plugin (MIDI effect) for non-MTS-ESP synths — applies pitch bend/MPE retuning directly before the synth. Needs a Max for Live wrapper for Ableton (same approach as ODDSound MTS-ESP MIDI Client). Works standalone as a MIDI plugin in other DAWs

- Make presets and slider values **MIDI-mappable and automatable** (JUCE AudioProcessorParameter integration)

- Add **cache status icons** to tuning system dropdown items (aligned far-right) showing whether the data is already cached or needs to be downloaded

- Make Slider thumbs create pitchbend when being moved up or down but keep the snap to markers so user can retune intervals to their liking whilst also being able to access the proper tuning if needed.

- Make sure all plugin settings and preset list are saved with the plugin so that on reload they are the same as where user left off

## Performance

- ~~**Investigate slow cache loading**~~ — DONE: lazy loading + incremental saves. Startup scans filenames only (2ms); entries deserialized on demand. Also fixed cache not persisting (was destructor-only)
