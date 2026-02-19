## Features
- Fix the Max for Live Receiver for Ableton 

- Add an internal oscillator (triangle wave) with ADSR and allow computer keyboard input so user can play the tuner without needing MIDI input if desired. This should be activated by a button named "Oscillator" or "Osc" next to the MTS-ESP, MPE and Pitch Bend buttons. The oscillator should be **enableable alongside** the other output modes (not mutually exclusive) so the user can play both the internal oscillator and their synth simultaneously to verify the synth is producing the correct tuning

- Make presets and slider values **MIDI-mappable and automatable** (JUCE AudioProcessorParameter integration)

- Add **cache status icons** to tuning system dropdown items (aligned far-right) showing whether the data related to each tuning system is already cached or needs to be downloaded (this includes maqamat and transpositions not just the tuning system data)

- Make Slider thumbs create pitchbend when being moved up or down but keep the snap to markers so user can retune intervals to their liking whilst also being able to access the proper tuning if needed.

- Make sure all plugin settings and preset list persist with the plugin so that on reload they are the same as where user left off

- Update the Receiver UI so it matches the Transmitter

- Update all references to Arabic Maqam Tuner including C++ class names and any styling etc... we should have none, everything should be Tanghīm. And change our project directory name to "tanghim".

_ Double Click range slider to reset it should only work when reclicking the range slider thumb

- sometimes changing maqamat whilst playing causes midi notes to hang. we should find out why and fix.

- list maqam transpositions in dropdown menu based on their pitch class order from low to hight (the maqam tonic can be referenced against our pitch classes index in the tuning system data for the correct order)

- maqam transpositions menu: (base) should be (qarār) instead



## Performance

- ~~**Investigate slow cache loading**~~ — DONE: lazy loading + incremental saves. Startup scans filenames only (2ms); entries deserialized on demand. Also fixed cache not persisting (was destructor-only)


/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client MPE.amxd

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client.amxd