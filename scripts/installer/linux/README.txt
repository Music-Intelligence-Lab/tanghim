Tanghim — Linux installation
=============================

1. Extract this archive (you've already done this if you're reading
   this file from the extracted folder).

2. Run the installer:

     ./install.sh

   This copies the VST3 plug-ins to ~/.vst3 and the CLAP plug-ins
   to ~/.clap (no sudo needed for these), and installs the MTS-ESP
   shared library (libMTS.so) to /usr/local/lib. The library install
   needs write access to /usr/local/lib — the script uses sudo if
   available, or prints the manual command if it can't.

   The MTS-ESP library is REQUIRED: without it, Tanghim cannot
   broadcast tuning to any synth and nothing is retuned.

3. Rescan plug-ins in your DAW (Reaper, Bitwig, Ardour, etc.).

Manual install
--------------
If you prefer, copy the contents of plugins/ into your plug-in
folders yourself:

    plugins/Tanghim.vst3/            →  ~/.vst3/
    plugins/Tanghim Receiver.vst3/   →  ~/.vst3/
    plugins/Tanghim.clap             →  ~/.clap/
    plugins/Tanghim Receiver.clap    →  ~/.clap/

And install the MTS-ESP shared library (REQUIRED for tuning):

    sudo install -m 0755 plugins/libMTS.so /usr/local/lib/libMTS.so

Ableton Live users
------------------
Ableton doesn't run on Linux, so the M4L Receiver device isn't
included in this archive.
