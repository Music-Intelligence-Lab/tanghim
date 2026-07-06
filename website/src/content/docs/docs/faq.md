---
title: FAQ
description: Answers to common questions about Tanghīm, maqām tuning, plugin formats, and getting sound out.
---

Short answers to the questions that come up most often. Where a topic has its own
page, the answer links there rather than repeating it.

## What is Tanghīm?

Tanghīm is a free, open-source MIDI plugin that applies Arabic maqām tuning to your
instruments. It draws its tuning data from the [Digital Arabic Maqām Archive
(DiArMaqAr)](https://diarmaqar.net) and works across any DAW as a VST3, AU, or
CLAP plugin. See [Introduction](/docs/getting-started/introduction) for the fuller
picture.

## Is it free? Can I see the source?

Yes. Tanghīm is free and open source, released under CC BY-NC-SA 4.0. The full
source is on [GitHub](https://github.com/Music-Intelligence-Lab/tanghim).

## What are the two plugins (Transmitter and Receiver)?

The project ships as two complementary plugins. **Tanghim (the Transmitter)** is
the main plugin with the full interface; it broadcasts the tuning over MTS-ESP.
**Tanghim Receiver** is a lightweight MIDI effect that reads that broadcast and
converts it to pitch bend (MPE or mono) for instruments that don't support MTS-ESP
natively. Most setups only need the Transmitter; you add the Receiver when your
instrument can't read MTS-ESP itself. See [Tuning Methods](/docs/tuning/methods)
and [the Receiver](/docs/tuning/receiver).

## What is a maqām?

A maqām is a melodic mode from the music of the Arabic-speaking region. Beyond a
scale of notes, it carries a specific intonation: each degree has a precise pitch,
and some of those pitches fall between the twelve equal-tempered keys of a standard
keyboard. Tanghīm reproduces those exact pitches.

## What is a "quarter-tone", and why not just use 12-EDO?

Standard keyboards divide the octave into twelve equal steps (12-EDO). Arabic maqām
uses pitches that sit between those steps, often described loosely as "quarter-tones",
though the real intervals are specific ratios, not an even quartering. Playing a
maqām on a plain 12-EDO keyboard flattens those in-between pitches to the nearest
key and loses the character of the mode. Tanghīm retunes the notes so they land
where the maqām actually places them.

## What is the difference between tanghīm and "tasyīk" or "tashrīq"?

*Tasyīk* / *tashrīq* is the common shortcut of lowering a single note by roughly 50
cents to a "quarter-tone", enough to suggest an Eastern colour. Full maqām tuning
(tanghīm) is different: it gives every degree its precise ratio, not just one
altered note. Tanghīm applies the complete intonation rather than the shortcut.

## Where does the tuning data come from?

From the [Digital Arabic Maqām Archive (DiArMaqAr)](https://diarmaqar.net), an
open-source repository of historically documented tuning systems and maqāmāt, each
with full bibliographic attribution. Tanghīm queries the archive's API and caches
the data locally, so the pitches you play match the archive's source-attributed
values.

## Do I need an internet connection?

Only the first time you select a particular tuning system. After that the data is
stored in a local cache and works offline. An update button tells you when the
archive has changed.

## What is MTS-ESP, and why does Tanghīm use it?

MTS-ESP is an open tuning protocol (by ODDSound) that broadcasts a 128-note
frequency table over shared memory. Any instrument that supports MTS-ESP retunes
itself automatically, with no extra plugin and no manual setup, and follows changes
of maqām in real time. It's the simplest path when your instrument supports it. See
[Tuning Methods](/docs/tuning/methods).

## My synth doesn't support MTS-ESP. What do I do?

Use the **Tanghim Receiver**, which turns the tuning into per-note pitch bend. It
has two modes: MPE (MIDI Polyphonic Expression) for MPE-capable instruments, and
Mono Pitch Bend for single-channel synths. See [the Receiver](/docs/tuning/receiver).

## How do I use Tanghīm in Ableton Live?

Ableton Live does not host third-party MIDI-effect plugins in any format, so Tanghīm
ships two Max for Live devices (an MPE Receiver and a Mono Pitch Bend Receiver) that
deliver the tuning inside Live. See the [Ableton Live guide](/docs/ableton).

## Which plugin format should I install: VST3, AU, or CLAP?

Install whichever your DAW supports; they are functionally the same. VST3 works
almost everywhere, AU is the native macOS format (Logic, GarageBand), and CLAP is
the newer open format supported by a growing set of hosts. AU is macOS-only. See
[Installation](/docs/getting-started/installation) for per-OS locations.

## Can I switch maqām while I'm playing?

Yes. You can change maqām or transposition live from the interface, and the tuning
follows every connected instrument in real time. You can also map presets to MIDI
notes or Program Change messages for hands-free switching. See
[Presets](/docs/interface/presets) and
[MIDI preset mapping](/docs/performance/midi-preset-mapping).

## Can I adjust a tuning by ear?

Yes. Every degree is editable: nudge any note with the sliders to taste, rather than
being bound to the archive's values, and save the result as a preset or a `.tanghim`
state file. See the [Slider Bank](/docs/interface/slider-bank) and
[state files](/docs/performance/state-files).
