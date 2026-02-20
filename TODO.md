## Features
- Fix M4l Receiver Patch layout UI


- Add an internal oscillator (triangle wave) with ADSR and allow computer keyboard input so user can play the tuner without needing MIDI input if desired. This should be activated by a button named "Oscillator" or "Osc" next to the MTS-ESP, MPE and Pitch Bend badges. The oscillator should be **enableable alongside** the other output modes (not mutually exclusive) so the user can play both the internal oscillator and their synth simultaneously to verify the synth is producing the correct tuning

- Make presets and slider values **MIDI-mappable and automatable** (JUCE AudioProcessorParameter integration)

- Add **cache status icons** to tuning system dropdown items (aligned far-right) showing whether the data related to each tuning system is already cached or needs to be downloaded (this includes maqamat and transpositions not just the tuning system data)

- Make Slider thumbs create pitchbend when being moved up or down but keep the snap to markers so user can retune intervals to their liking whilst also being able to access the proper value if needed.

_ Double Click range slider to reset it should only work when reclicking the range slider thumb

- sometimes changing maqamat whilst playing causes midi notes to hang. we should find out why and fix.

- I need you to document your research about enabeling "tuning" in Max and M4L devices in our diary. This will be useful for later so we can create a guide for how to do it and save ourselves this work in the future.

- Update all references to Arabic Maqam Tuner including C++ class names and any styling etc... we should have none, everything should be Tanghīm. And change our project directory name to "tanghim".


## Performance

- When our Transmitter plugin window is in "focus" the performance becomes sluggish, the midi data has timing issues... I can't tell exactly what's going on but I can hear and feel it. Having our Transmitter in focus should not be detrimental to performance in any way. 

- ~~**Investigate slow cache loading**~~ — DONE: lazy loading + incremental saves. Startup scans filenames only (2ms); entries deserialized on demand. Also fixed cache not persisting (was destructor-only)


## Paths
/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client MPE.amxd

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client.amxd