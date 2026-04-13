Tanghim — Linux installation
=============================

1. Extract this archive (you've already done this if you're reading
   this file from the extracted folder).

2. Run the installer:

     ./install.sh

   This copies the VST3 plug-ins to ~/.vst3 and the CLAP plug-ins
   to ~/.clap. No sudo is required.

3. Rescan plug-ins in your DAW (Reaper, Bitwig, Ardour, etc.).

Manual install
--------------
If you prefer, copy the contents of plugins/ into your plug-in
folders yourself:

    plugins/Tanghim.vst3/            →  ~/.vst3/
    plugins/Tanghim Receiver.vst3/   →  ~/.vst3/
    plugins/Tanghim.clap             →  ~/.clap/
    plugins/Tanghim Receiver.clap    →  ~/.clap/

Ableton Live users
------------------
Ableton doesn't run on Linux, so the M4L Receiver device isn't
included in this archive.
