## Features

- Add an internal oscillator (triangle wave) with ADSR and allow computer keyboard input so user can play the tuner without needing MIDI input if desired. This should be activated by a button named "Oscillator" or "Osc" next to the MTS-ESP, MPE and Pitch Bend badges. The oscillator should be **enableable alongside** the other output modes (not mutually exclusive) so the user can play both the internal oscillator and their synth simultaneously to verify the synth is producing the correct tuning

- add a global tuning UI element that allows to change the "reference frequency" of the entire tuning system up or down... it should have a knob and a text box so user can be precise or work by ear. In common practice this is referred to as "Master Tune" but that is fucking racist, also incorrect. Call it Reference Freq. The number displayed should be the frequency in Hz of the tuning systems's starting note name or the first pitch class in its octave 1, and although the modification will probably have to be in cents, we should display Hz.

- in status bar, "Preset MIDI" should say, "Preset MIDI Mapping via:"

- still need to fix the automation not refreshing fast enough in the transmitter VST UI

- Add **cache status icons** to tuning system dropdown items (aligned far-right) showing whether the data related to each tuning system is already cached or needs to be downloaded (this includes maqamat and transpositions not just the tuning system data)

- add a button next to update in the status bar to clear cache

- sometimes changing maqamat whilst playing causes midi notes to hang. we should find out why and fix.

- I need you to document your research about enabeling "tuning" in Max and M4L devices in our diary. This will be useful for later so we can create a guide for how to do it and save ourselves this work in the future.

- Update all references to Arabic Maqam Tuner including C++ class names and any styling etc... we should have none, everything should be Tanghīm/Tanghim. And change our project directory name to "tanghim".


## Performance



## Paths
/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client MPE.amxd

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client.amxd