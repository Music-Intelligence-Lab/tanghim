---
title: MIDI File Export
description: Export the current maqam scale as a Standard MIDI File.
---

You can export the current maqam scale as a Standard MIDI File for use in other applications.

## How to Export

1. Click the **MIDI drag button** in the status bar.
2. Drag it into your DAW or file manager.

The file is saved to `~/Library/Tanghim/midi-export/` (macOS) or the equivalent path on Windows/Linux. Filename format: `maqamname_(PAOname-IPN-solfege).mid`.

The exported file contains all scale degrees played simultaneously as a chord (SMF Type 0, one quarter note).

## Program Change Embedding

If a **preset is active** when you export, a MIDI Program Change message is embedded at the start of the file. When the clip plays back through Tanghim, the preset will automatically activate before the notes sound.

The MIDI drag button is always visible in the status bar — it is inactive (grey) when no maqam is selected, and active (gold) when a maqam is selected.
