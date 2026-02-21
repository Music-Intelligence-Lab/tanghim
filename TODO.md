## Features

- Add an internal oscillator (triangle wave) with ADSR and allow computer keyboard input so user can play the tuner without needing MIDI input if desired. This should be activated by a button named "Oscillator" or "Osc" next to the MTS-ESP, MPE and Pitch Bend badges. The oscillator should be **enableable alongside** the other output modes (not mutually exclusive) so the user can play both the internal oscillator and their synth simultaneously to verify the synth is producing the correct tuning

- Make presets and sliders **MIDI-mappable and automatable** (JUCE AudioProcessorParameter integration)

- when a maqam is selected and its tuning sliders are modified, do not deselect it's degrees.. keep them selected but add an asterix * to the maqam name, so it can be saved in the presets as a "modified" version of that maqam.

- Add **cache status icons** to tuning system dropdown items (aligned far-right) showing whether the data related to each tuning system is already cached or needs to be downloaded (this includes maqamat and transpositions not just the tuning system data)

- move slider IPN name under cents value, add solfege under it (we already use it in the transposition dropdown) and then the PAO note name should stay as the last item so it can have multiple lines without shifting other items.

- ~~In the status bar add a badge/button that the user can click and drag onto their daw or their desktop that is a midi file rendering of the IPN notes used in a maqam . it's format should be like this sample midi file for maqam rast: maqām_rāst_al-rāst_C3_Do3.mid~~ **DONE** - Native MIDI drag button in status bar (Feb 21 2026)

- sometimes changing maqamat whilst playing causes midi notes to hang. we should find out why and fix.

- I need you to document your research about enabeling "tuning" in Max and M4L devices in our diary. This will be useful for later so we can create a guide for how to do it and save ourselves this work in the future.

- Update all references to Arabic Maqam Tuner including C++ class names and any styling etc... we should have none, everything should be Tanghīm. And change our project directory name to "tanghim".


## Performance



## Paths
/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client MPE.amxd

/Users/khyamallami/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/MTS-ESP M4L MIDI Effects/MTS-ESP MIDI Client.amxd