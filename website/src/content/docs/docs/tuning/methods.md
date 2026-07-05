---
title: Tuning Methods
description: "MTS-ESP, MPE, and Mono Pitch Bend: how each method works and which to use."
---

Tanghīm supports three methods for delivering maqam tuning to your instruments. The right choice depends on what your instrument supports. The top bar shows status badges for each active method; these are indicators, not toggles.

## MTS-ESP (Teal Badge)

MTS-ESP is an open protocol that broadcasts a 128-note frequency table. Instruments that support MTS-ESP retune themselves automatically, with no Receiver plugin needed.

- **Best for:** Software synths with built-in MTS-ESP support
- **How it works:** Tanghīm broadcasts tuning data via shared memory. Any MTS-ESP-compatible instrument running on the same machine picks it up instantly.
- **Compatible instruments include:** Surge XT, Vital, Diva (enable MTS-ESP in settings), Pianoteq, ZynAddSubFX, and many others.
- **Setup:** Just load Tanghīm and your instrument; the connection is automatic.

## MPE: MIDI Polyphonic Expression (Blue Badge)

MPE delivers tuning as per-note pitch bend messages across MIDI channels 2–16. Each note gets its own channel, allowing polyphonic tuning with independent pitch bend per voice.

- **Best for:** Polyphonic instruments that support MPE but not MTS-ESP, including many hardware synths and some software instruments.
- **How it works:** The Tanghīm Receiver plugin sits before your instrument in the signal chain and converts the MTS-ESP tuning into MPE pitch bend messages.
- **Default pitch bend range:** 48 semitones (set this to match your instrument's MPE pitch bend range).
- **Setup:** Load Tanghīm Receiver before your instrument and select MPE mode.

## Mono Pitch Bend (Green Badge)

Mono Pitch Bend delivers tuning as 14-bit pitch bend on a single MIDI channel. This is the most universally compatible method: nearly every synth responds to pitch bend.

- **Best for:** Monophonic patches, instruments without MPE support, hardware synths, and any instrument that only responds to standard pitch bend.
- **How it works:** The Tanghīm Receiver calculates the required pitch bend for each note and sends it on the same channel. Includes a note stack with last-note priority for legato playing.
- **Default pitch bend range:** 2 semitones (set this to match your instrument's pitch bend range).
- **Setup:** Load Tanghīm Receiver before your instrument and select Mono PB mode.

## Which Method Should I Use {#which-method-should-i-use}

| Your instrument | Recommended method |
|---|---|
| Software synth with MTS-ESP support (Surge XT, Vital, Pianoteq, etc.) | **MTS-ESP**: zero setup, polyphonic, highest precision |
| Software synth with MPE support but no MTS-ESP | **MPE**: polyphonic tuning via Receiver |
| Hardware synth with MPE support | **MPE**: polyphonic tuning via Receiver |
| Hardware synth (standard MIDI only) | **Mono PB**: works with any pitch-bend-capable instrument |
| Monophonic synth or mono patch | **Mono PB**: simplest, most compatible |
| Instrument with no pitch bend at all | **MTS-ESP** is the only option (if supported) |

You can use multiple methods simultaneously, for example MTS-ESP for your main synths and a Receiver in Mono PB mode for a hardware synth, all driven by the same Tanghīm instance.
