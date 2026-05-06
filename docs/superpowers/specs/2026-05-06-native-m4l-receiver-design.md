# Native M4L Receiver — Design Spec

**Date:** 2026-05-06
**Status:** Awaiting user review

## Problem

The current M4L Receiver hosts the Tanghim Receiver VST3 inside Max's `vst~` object. Because VST3 plugins cannot emit MIDI through `vst~` in Max for Live, we built a parameter-bridge architecture: the C++ side exposes 128 cents-per-note `AudioParameterFloat` values, the M4L patch polls those parameters via `vst~`, and a JavaScript object (`mts_midi_effect.js`) does the per-Note-On MPE channel allocation and pitch-bend emission inside Max.

This architecture has produced a recurring trail of bugs:

- Note timing jitter under same-pitch MPE repetition (fixed via `metro 1000` polling rate).
- MPE channel-allocation leaks (fixed via per-slot `channelNote`/`channelActive` arrays).
- A failed event-driven bridge attempt: investigation proved `vst~` outlet 3 does not emit when `setValueNotifyingHost` is called from `processBlock`. The bridge is fundamentally polling-bound.
- Recording-time MIDI jitter that scales with Live's audio buffer size. Cycling '74 documentation confirms the JS engine always runs in the low-priority thread and cannot be moved. This is unfixable while JS is in the per-Note-On path.
- The `vst~` object loses its plugin instance after Live audio buffer-size changes; recovery requires deleting and re-adding the device.

The shared root cause is the architecture itself: `vst~` hosting a VST3 (which can't emit MIDI), forcing per-note MIDI logic into JS (which runs on the low-priority thread).

ODDSound — the authors of MTS-ESP — publish a free, open-source **MTS-ESP Max Package** (https://github.com/ODDSound/MTS-ESP-Max-Package, 0BSD license). It provides native Max externals (`MTS-ESP.mtof`, `MTS-ESP.mtof~`, `MTS-ESP.ftom`) that read live MTS-ESP tuning data directly from shared memory without any plugin host. Combined with stock Max objects (`poly`, `coll`, `expr`, `xbendout`) for MIDI processing, this lets us replace the entire `vst~` + JS architecture with a pure-native Max patch that runs MIDI on the high-priority scheduler thread.

This was considered earlier in the day but rejected in favour of a VST2 build mirroring ODDSound's own MIDI Client M4L architecture. We then reverted that decision because: (a) Steinberg ceased issuing VST2 developer licenses in 2018 and we have no legal redistribution right; (b) the VST2 path inherits the `vst~`-loses-plugin-on-buffer-change bug. The native MTS-ESP Max Package path avoids both problems at the cost of more patch-side engineering.

## Goal

Replace the M4L Receiver with two thin, native-Max devices (`Tanghim MPE Receiver.amxd` and `Tanghim Mono PB Receiver.amxd`) that use ODDSound's MTS-ESP Max Package for tuning data and stock Max objects (`poly`, `coll`, `expr`, `xbendout`) for MIDI processing. No `vst~`, no JavaScript, no VST2.

## Scope

- **In scope:**
  - Two new `.amxd` files (`Tanghim MPE Receiver.amxd`, `Tanghim Mono PB Receiver.amxd`).
  - One supporting Python generator script (`m4l/generate_patch.py`, replacing the current single-device version), with shared topology helpers and per-device entry points.
  - The ODDSound MTS-ESP Max Package externals (`MTS-ESP.mtof.mxo` on macOS, `MTS-ESP.mtof.mxe64` on Windows) shipped alongside the `.amxd` files in the installer.
  - Tag the current commit (`4f12931`) as `last-vst3-bridge-amxd` for rollback (already done).
  - Update CI release workflow to bundle the ODDSound externals + new `.amxd` files for both macOS and Windows.
  - Update macOS `.pkg` and Windows installer to drop both `.amxd` files and the externals into the user's Live library.
  - Update CLAUDE.md and write a diary entry.
- **Out of scope:**
  - Changes to the Tanghim Transmitter (still broadcasts MTS-ESP exactly as it does today).
  - Changes to the Tanghim Receiver standalone VST3 / AU / CLAP (still ships unchanged for hosts that aren't Live — Logic, REAPER, Cubase, Bitwig, etc.).
  - Building any VST2.
  - The MTS-ESP MIDI Client manual / documentation tooling.

## Non-Goals

- We do not replace the MTS-ESP `libMTSClient` in our existing C++ Receiver. That stays. We are only changing what the *M4L wrapper* does.
- We do not bundle ODDSound's externals *inside* the frozen `.amxd` (`dlst`-directory embedding for `.mxo` bundle directories is uncharted territory). They ship alongside the `.amxd` files and Live's Max-package search path picks them up.
- We do not build a single combined `.amxd` with a runtime mode switch. Live's `is_mpe: 1` is a patcher-level flag, not a runtime mode — ODDSound's own MIDI Client M4L splits for the same reason.
- We do not implement an MTS-ESP master scale-name readout, a filter-note opt-out, or other UI features beyond what the current device exposes.

## Architecture Overview

Each `.amxd` is a pure Max patch. The skeleton:

```
[live.midiin]
       │
       ▼
   [midiparse]   (or equivalent — split message types)
       │
       ├── Note On  ──────────────────────────────────────────┐
       ├── Note Off ──────────────────────────────────────────┤
       ├── Pitch Bend (user wheel) ────────────────────────────┤
       └── (other passthrough: CC, aftertouch, program change)─┤
                                                              │
                                       ┌──────────────────────┘
                                       ▼
                       ┌──────────────────────────────────┐
                       │   Per-device MIDI processing     │
                       │   MPE: [poly] voice allocator    │
                       │   Mono PB: [coll] note stack     │
                       └──────────┬───────────────────────┘
                                  │ (note, channel, cents)
                                  ▼
                       [MTS-ESP.mtof] ← bang to query cents
                                  │
                       [expr] cents → 14-bit PB value
                                  │
                       Combine with user PB wheel (clamp)
                                  │
                       [xbendout ch] (PB) then [noteout ch] (Note On/Off)
                                  │
                            [midiformat → live.midiout]
```

`MTS-ESP.mtof` reads tuning data from the same MTS-ESP shared memory the Tanghim Transmitter is broadcasting to — no parameter bridge, no `vst~`. All MIDI processing runs on Max's high-priority scheduler thread, eliminating the JS-induced recording jitter.

## Components

### Both devices share

**MIDI input.** `[live.midiin]` for incoming MIDI from the Live track, then `[midiparse]` to split into Note On/Off, PB, CC, aftertouch streams.

**MTS-ESP query.** `[MTS-ESP.mtof]` is the only ODDSound external used. Outlets per query: `[freq, ratio, semitones, filter]`. We use:
- `semitones × 100` → cents for PB calculation
- `filter` → gate Note Ons when the master marks a note unmapped

**Cents → 14-bit PB.** `[expr int(8192 + ($f1 / ($i2 * 100.)) * 8191)]` where `$f1` is cents, `$i2` is the device's PB range. Clamped via `[clip 0 16383]`.

**User PB wheel combining.** Per channel, store `userPbOffset = userPb - 8192`. Final emitted PB = clamp(microbendPb + userPbOffset, 0, 16383). Implemented with `[coll userPbState]` and `[expr]`.

**Held-note retune at 10 Hz.** `[metro 100]` ticks. Each tick iterates a `[coll heldNotes]` (keyed by note number, value = channel + last-known PB). For each entry: bang `MTS-ESP.mtof`, recompute combined PB, emit if changed by > 0.5 cent epsilon. Idle (no notes held) → zero scheduler activity.

**Filter-note routing.** Note On is gated by the `filter` outlet of `MTS-ESP.mtof`. When `filter == 0`, the Note On is dropped silently.

**Connection-status indicator.** `[live.text]` shows "Connected" / "No master". Detection: bang `MTS-ESP.mtof` for note 60 at 1 Hz; if `filter == 1` AND any retuning has been seen recently, treat as connected. Best-effort, since `MTS-ESP.mtof` falls back silently to 12-TET when no master is present.

**Pass-through.** CC, aftertouch, program change pass through unchanged. The user's pitch wheel (channel 1 in MPE manager mode, or the input channel in Mono PB) feeds the combining logic above; it does NOT pass straight through.

### MPE Receiver only

**Patcher metadata flag:** `is_mpe: 1` on the patcher. Without this Live collapses output to channel 1.

**Channel allocator:** `[poly @voices 15 @steal 1]` wrapping a per-voice subpatch that emits PB then Note On then Note Off on its assigned channel. `[thispoly~]` exposes the voice index inside the subpatch; **MPE channel = voice_index + 2** (channel 1 is the manager).

**MPE manager (channel 1):** Channel 1 carries the user's pitch wheel input only. We do not emit notes on channel 1.

**MPE zone configuration message:** On device load, `[loadbang] → [delay 50]` sends an MPE Configuration RPN (lower zone, 15 voices) on channel 1.

**Default PB range:** 48 semitones.

**Same-pitch overlap:** verified during implementation — confirm `poly @steal 1` allocates a fresh voice slot for repeated same-pitch Note Ons (so a held note 60 keeps sounding when a new note 60 arrives on a new MPE channel). If `poly`'s allocator doesn't deliver this, fall back to a `[coll]` + `[expr]` allocator mirroring the existing C++ `MpePitchBendProcessor` design.

### Mono PB Receiver only

**No `is_mpe` flag.** Output goes on a single channel (channel 1).

**Note stack:** `[coll noteStack]` keyed by stack position, value = `(note, semitones)`. Last-note-priority legato:
- Note On: push entry, emit PB then Note On for this note on channel 1.
- Note Off: remove entry. If the released note was top of stack and stack is non-empty, emit PB for the new top-of-stack note (legato — no Note On, just PB update).
- PB wheel: combined into emitted PB as above.

**No PB reset on Note Off.** Resetting PB to 8192 on Note Off causes audible snap during the synth's release tail (already documented in CLAUDE.md from the existing JS implementation).

**Default PB range:** 2 semitones.

## UI

Minimal, matches the spirit of the current device.

| Element | MPE | Mono PB |
|---|---|---|
| Title | "Tanghīm MPE Receiver" | "Tanghīm Mono PB Receiver" |
| PB Range numbox | `live.numbox`, 1–96, default 48 | `live.numbox`, 1–96, default 2 |
| Status indicator | `live.text`: "Connected" / "No master" | same |

The PB Range numbox is a Live-automatable parameter exposed via `live.numbox`. No mode dropdown (each device is single-mode by construction). No scale-name readout, no filter-note toggle, no extras.

## Distribution

The ODDSound MTS-ESP Max Package externals must be available to Live for the patch to load.

**Decision: ship them alongside the `.amxd` files**, dropped into the user's Live User Library by the installer. Embedding the externals into the frozen `.amxd` (`dlst` directory mechanism) is not feasible for `.mxo` bundle directories on macOS — the freeze mechanism that works for our existing JS file does not extend to opaque bundle directories.

Layout:

```
~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim/
├── Tanghim MPE Receiver.amxd
├── Tanghim Mono PB Receiver.amxd
└── MTS-ESP.mtof.mxo/      ← ODDSound external (macOS)
```

On Windows the same path under `%USERPROFILE%\Documents\Ableton\User Library\…` and `MTS-ESP.mtof.mxe64` (single file).

The externals are sourced from a pinned commit of the ODDSound MTS-ESP Max Package repo and either:
- (a) checked into our repo under `m4l/externals/` (small, header-only would be different — these are compiled binaries, ~100KB each), or
- (b) downloaded by the build script at release time.

**Decision deferred to implementation:** start with (a) — committed binaries — for reproducibility. If repo size becomes a concern, switch to (b).

## Migration & Rollback

1. The current commit `4f12931` is already tagged `last-vst3-bridge-amxd` (pushed to origin) for rollback.
2. The implementation plan replaces the contents of `m4l/`:
   - **Delete:** `Tanghim Receiver.amxd`, `Tanghim Receiver.maxpat`, `mts_midi_effect.js`, the existing single-mode `generate_patch.py`.
   - **Create:** `Tanghim MPE Receiver.{amxd,maxpat}`, `Tanghim Mono PB Receiver.{amxd,maxpat}`, a new `generate_patch.py` (or two per-device generators with a shared helper module), `externals/MTS-ESP.mtof.mxo/`.
   - The replacement happens in one logical commit; no parallel coexistence.
3. The C++ Receiver is untouched. No new build target. No CMake changes.
4. CI release workflow updated to bundle the new files; macOS `.pkg` and Windows installer updated to drop them into the user library.
5. CLAUDE.md "Max for Live Wrapper" section is rewritten. JS-related gotchas (midiparse 7-bit conversion, JS default initialization, parameter-poll rate, MPE allocation in JS) are removed. The MTS-ESP Max Package architecture is documented.
6. A new diary entry (`diary/2026-05-06.md`) captures the dead ends (event-driven bridge, VST2 attempt) and why the native package path was chosen.

## Verification Plan

1. **Devices load.** Drop both `.amxd` files into a fresh Live session. They appear in the device chain without errors. The Max Console shows no "external not found" complaints (confirms the externals are resolving).
2. **Tuning correctness.** With Tanghim Transmitter on track A and **MPE Receiver** on track B feeding a soft synth in MPE mode, select Maqam Hijaz. Play a chromatic scale C3–C4. Pitches must match the maqam's tuning, audibly distinct from 12-TET on the microtonal degrees.
3. **Mono PB mode.** Replace MPE Receiver with **Mono PB Receiver** on track B feeding a single-channel synth (PB range = 2). Play a melody. Each note in tune. Hold A, hold B, release A — note B continues at its tuned pitch (legato, no snap).
4. **Same-pitch MPE allocation.** With MPE Receiver: trigger note 60 → 60 → 60 with a fast arpeggiator and overlap (sustain-pedal or note length > arp interval). Each successive 60 lands on a fresh MPE channel; previously-sounding 60s continue ringing at their original PB.
5. **Filter-note.** Configure the Transmitter with a maqam that filters certain notes. Play those notes. The Receiver drops them — no sound.
6. **PB wheel combining.** Hold a tuned note; move controller pitch wheel up. Pitch bends FROM the maqam-tuned pitch (additive), does NOT jump to 12-TET first.
7. **Held-note retune during automation.** Hold a chord. Change the maqam at the Transmitter. The chord's pitches must update within ~100ms (10 Hz held-note poll).
8. **Recording jitter — HEADLINE METRIC.** At Live audio buffer 1024, record a 4-note arpeggio at 120bpm 16th notes. Recorded clip note start times must be on-grid (Live's normal sub-millisecond jitter is fine), NOT the random ±5–12ms displacement seen with the old JS-based device. **This is the headline success criterion.**
9. **Buffer-change survival.** With the device active and playing a note loop, change Live's audio buffer 256 → 1024 → 256. The device must continue passing tuned MIDI without requiring delete-and-readd. (No `vst~`, so this should be trivially true — but verify, because it was the second motivating bug.)
10. **MTS-ESP MIDI Client coexistence.** With Tanghim Transmitter on track A, drop ODDSound's own MTS-ESP MIDI Client M4L on track C with a synth, plus our **MPE Receiver** on track D with a different synth. Both must produce the maqam tuning correctly.
11. **Idle scheduler load.** With no notes held, observe Max's CPU meter. Should be near zero — `[metro 100]` ticks but iterates an empty `coll`.

## Open Items (settled in implementation, not now)

- **Where to source the externals binaries.** Pin a specific commit/tag of the ODDSound repo. Confirm we can use macOS universal builds (arm64 + x86_64) — Live's plugin scanner runs x86_64 under Rosetta on Apple Silicon, same constraint that bit us with VST3 builds.
- **`poly` allocator behaviour for same-pitch overlap.** Verify before relying on it. Fall back to `coll`-based allocator if `poly` doesn't fan voices.
- **Connection-status detection heuristic.** The `MTS-ESP.mtof` external falls back to 12-TET silently when no master is present. Sentinel-note ping or accept "best-effort" status reporting.
- **MPE Configuration RPN.** Verify the byte sequence is correct for a 15-voice lower zone.

## Risks

- **`poly` allocator fans voices in an unhelpful way for same-pitch overlap.** Mitigation: implementation plan includes a fallback `coll` allocator path.
- **`MTS-ESP.mtof` rate-limits its own queries internally.** Unlikely (it's a thin shim over `libMTSClient`) but verify by stress-testing the held-note poll.
- **Recording jitter is NOT eliminated by removing JS.** If the headline test fails, the root cause is something other than JS scheduling — possibly Live's audio-block quantization at the M4L/Live MIDI boundary (which we previously researched as inherent and unfixable). Documentation already acknowledges this; we'd be no worse off than today, but the rebuild's headline win would not materialize.
- **Mono PB note-stack edge cases.** Stack operations (push/pop with same-note-twice handling) are easier to get wrong in `coll`/`zl` than in C++. Mitigate via explicit verification step #3 above.

## Decisions Locked

- **Path:** Replace `vst~`+JS architecture with native ODDSound MTS-ESP Max Package + stock Max objects.
- **Devices:** Two `.amxd` files (`Tanghim MPE Receiver.amxd`, `Tanghim Mono PB Receiver.amxd`). Required because Live's `is_mpe: 1` is patcher-level, not runtime — same constraint that drove ODDSound's split.
- **JS:** None. All MIDI logic in native Max objects.
- **VST2:** Not built. The earlier exploration of VST2 was reverted.
- **`vst~`:** Not used in either `.amxd`.
- **PB wheel combining:** Kept (additive to microtuning, never overrides).
- **Polling:** Held-notes only at 10 Hz, plus immediate query on Note On.
- **UI:** Minimal — title + PB range numbox + status indicator per device.
- **C++ Receiver standalone (VST3/AU/CLAP):** Untouched, continues shipping for non-Live hosts.
- **ODDSound externals:** Shipped alongside `.amxd` files in the installer, dropped into user's Live library. Not embedded in the frozen `.amxd`.
- **Migration:** Old single-device `.amxd` + `.js` deleted in one logical commit after new devices are verified working. Rollback available via `last-vst3-bridge-amxd` tag.
