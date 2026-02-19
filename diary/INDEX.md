# Production Diary — Arabic Maqam Tuner

A chronological record of building the Arabic Maqam Tuner VST plugin, from concept to completion. Written for reference when writing the post-project article about the build process and DiArMaqAr integration.

## Project

**What:** A cross-platform VST3/AU/CLAP microtuning plugin that fetches tuning data from the DiArMaqAr REST API and applies Arabic maqam-based microtuning to MIDI via MTS-ESP, MPE, and 14-bit pitch bend.

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
