---
title: The Slider Bank
description: How pitch-class sliders work, snap markers, per-note overrides, and the range scroller.
---

The slider bank presents 12 sliders — one for each chromatic step of the octave — arranged in the standard Anglo-European chromatic order: C, C#, D, D# (Eb), E, F, F#, G, G# (Ab), A, A# (Bb), B. Each slider controls the tuning of its dedicated pitch class. The same tuning applies to that pitch class across all octaves unless a per-note override is set.

## How Pitch Classes Map to Sliders

Arabic maqām tuning systems define far more than 12 pitch classes per octave — for example, the modern Arabic system has 24 distinct pitches, while Ibn Sīnā's 11th-century system defines 17. Tanghīm resolves this by assigning each pitch class to the chromatic slider it is a **variant of**, based on Arabic musicological logic and the 12-tone chromatic system documented by al-Kindī (9th century).

The key principle: **each pitch is a variant of the chromatic note it modifies, not the chromatic note it is nearest to.** For example, the half-flat E (segāh, E−♭) lives on the E slider because it is a *lowered E*, not a raised E♭. A half-sharp F (tīk būselīk, F+♯) lives on the F slider because it is a *raised F*.

For example, in the 24-tone system starting on yegāh, the pitches between D and G map as follows:

| Slider | Snap markers (variants) | PAO names |
|--------|------------------------|-----------|
| **E** | E−♭ (half-flat), E♮ | segāh, būselīk |
| **F** | F♮, F+♯ (half-sharp) | chahārgāh, tīk būselīk |
| **F#** | F♯♮, F♯+♯ (half-sharp) | nīm ḥijāz, ḥijāz |
| **G** | G♮ | nawā |

When a slider has multiple variants, small **snap markers** on the slider track indicate each available tuning position.

## Snap Markers vs Free Tuning

Snap markers indicate the tuning positions defined by the selected tuning system. When you select a maqam, the sliders snap to these positions automatically. You are free to **drag any slider away from its marker** to adjust the tuning by ear.

Tuning systems in the database represent theoretical models, but in practice, performers constantly adjust intonation based on context, taste, and tradition. The sliders let you start from a theoretical reference and refine it to match what sounds right to you.

When you move a slider away from its snapped position, the thumb turns **cyan** and the maqam name shows an asterisk (`*`) suffix, indicating the tuning has been modified from its theoretical reference values.

## Adjusting Tuning: All Octaves vs Single Octave

**All octaves (drag)** — Drag a slider normally to adjust that pitch class across every octave at once. Modified sliders show a **cyan** thumb (maqam degrees) or **teal** thumb (non-degree notes).

**Single octave (Shift+drag)** — Hold **Shift** and drag a slider to adjust the tuning of that note in only the current octave, leaving the same pitch class in all other octaves unchanged. Per-note overridden notes show a **blue** thumb glow. This is useful when you need a note tuned differently in one register.

## Other Controls

**Variant selector** — Some pitch classes have multiple tuning variants (e.g. different sizes of segāh). Click the note name to cycle through them.

## Range Scroller

The horizontal range bar below the sliders lets you pan across the full MIDI range:

- **Drag** to scroll.
- **Double-click** to center on the maqam octave.
- **Magnetic snap** to C, G, and A tick marks.
