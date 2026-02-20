# Production Diary — Tanghīm

A chronological record of building the Tanghīm (formerly Arabic Maqam Tuner) VST plugin, from concept to completion. Written for reference when writing the post-project article about the build process and DiArMaqAr integration.

## Project

**What:** A cross-platform VST3/AU/CLAP MIDI tuning plugin that fetches tuning data from the DiArMaqAr REST API and applies Arabic maqam-based microtuning via MTS-ESP, MPE, and 14-bit pitch bend.

**Who:** Khyam Allami (concept, direction) + Claude (implementation)

**Stack:** JUCE 8 (C++), React + TypeScript + Vite (WebView UI), DiArMaqAr API

## Milestones

| # | Milestone | Status | Diary Entry |
|---|-----------|--------|-------------|
| 0 | Prerequisites (Context7, tooling) | Done | [2026-02-17](2026-02-17.md) |
| 1 | Project skeleton (builds, loads in DAW) | Done | [2026-02-17](2026-02-17.md) |
| 2 | Data model + API layer | Done | [2026-02-17](2026-02-17.md) |
| 3 | Tuning engine (MTS-ESP, MPE, PB) | Done | [2026-02-17](2026-02-17.md) |
| 4 | GUI (React WebView UI) | Done | [2026-02-17](2026-02-17.md) |
| 5 | State persistence + polish | Pending | — |

## Entries

- [2026-02-17](2026-02-17.md) — Project inception through Session 6: initial build, API integration, slider polish, output modes, per-note overrides, resizable slider bank, per-note MIDI activity, maqam selector & preset system
- [2026-02-18](2026-02-18.md) — Sessions 7–13: maqam list caching, preset compatibility, CustomSelect keyboard nav, preset button polish, window dimensions, IS_SYNTH instrument classification, code signing fix, audio.clear(), MTS-ESP client count in status bar, Transmitter/Receiver rename, Ableton MIDI routing notes, cache lazy loading + incremental saves, MTS-ESP Receiver plugin, Max for Live wrapper, thread safety fix, always-broadcast MTS-ESP, bendValue() UB crash fix, M4L parameter bridge architecture, py2max patch generation, MaxMSP MCP server setup
- [2026-02-19](2026-02-19.md) — Sessions 14–16: Remove output mode from Transmitter (MTS-ESP only), replace UI selector with read-only status display, M4L crash fix (plug_vst3), .amxd binary format, mode mapping fix, GitHub repo "tanghim" setup, plugin rename to "Tanghim" (ASCII constraint), live MTS-ESP status polling (2Hz), file-based receiver registry for type distinction (MTS-ESP/MPE/Mono PB badges), smooth slider scrolling, maqam octave centering, Receiver editor redesign, transposition dropdown with IPN+solfège labels
- [2026-02-20](2026-02-20.md) — Sessions 17–18: M4L Receiver working (MPE + Mono PB). Fixed VST3 parameter index offset (JUCE bypass param at index 0), added Ableton `is_mpe` flag for multi-channel MIDI output, fixed pitch bend snap-back on Note Off. Includes comprehensive guide for building microtuning M4L devices. Session 18: WKWebView keyboard focus investigation — Ableton's Computer MIDI Keyboard delayed after interacting with searchable dropdown. Six ObjC swizzling approaches attempted, none fully resolved. Root cause: WKContentView retains first responder after text input blur. Documented as known issue (only affects CMK, not external MIDI). Also fixed Release build resource provider (BinaryData filename mangling mismatch).
