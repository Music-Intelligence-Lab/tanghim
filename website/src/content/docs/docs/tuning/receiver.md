---
title: Tanghim Receiver
description: "The Receiver plugin: setup, modes, and pitch bend combining."
---

The Receiver is a lightweight MIDI effect plugin that enables MPE and Mono Pitch Bend tuning delivery. It reads the MTS-ESP tuning broadcast from Tanghīm and converts it to pitch bend messages that any instrument can understand. See [Tuning Methods](/docs/tuning/methods) for details on when to use MPE vs Mono PB.

## Setup

1. Load **Tanghīm** on one track (the transmitter).
2. Load **Tanghīm Receiver** before each instrument that needs pitch-bend-based tuning.
3. Choose **MPE** or **Mono PB** mode depending on your instrument (see the [comparison table](/docs/tuning/methods#which-method-should-i-use)).
4. Set the Receiver's pitch bend range to match your instrument's pitch bend range setting.
5. The Receiver automatically connects to the Transmitter, and the status display shows the active tuning system.

You can run multiple Receiver instances simultaneously, each in a different mode for different instruments.

## Pitch Bend Combining

If the player sends pitch bend wheel messages, the Receiver combines them with the tuning bend. This means your pitch wheel works normally on top of the maqam tuning.

## Note on Ableton Live

Ableton Live does not support VST3 MIDI effect plugins, so the Receiver cannot be loaded directly in Ableton. A Max for Live device is provided instead; see [Ableton Live](/docs/ableton) for details.

## Note on Logic Pro AU

Logic Pro suspends the audio thread of a MIDI effect AU track when no instrument is loaded downstream and the transport is stopped. In this state, the Receiver's status will display "No transmitter found" because the connection is established inside the audio processing callback. Loading a synth after the Receiver on the same track, or having MIDI flowing, will activate the track and the status will update to "Connected".
