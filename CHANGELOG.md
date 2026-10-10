# Changelog

All notable changes to Tanghīm are documented here.

This file is the **single source of truth** for release notes. The GitHub Release body and
the website changelog are both generated from it — never write release notes by hand in
either place. See [`RELEASING.md`](RELEASING.md).

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning is [Semantic Versioning](https://semver.org/): while in beta the minor stays
at `0.9` and each release bumps the **patch** (`0.9.1`, `0.9.2`, …). `1.0.0` marks the
first public release.

## [Unreleased]

## [0.9.1] - 2026-10-10

### Fixed

- **Maqām degrees were being silently dropped from the keyboard.** A maqām whose degrees
  shared a chromatic slot lost one of them — e.g. maqām bayyāt on nawā in al-Ṣabbāgh (1954)
  played a 6-note scale, with `tīk ḥiṣār` missing and the maqām marked as modified. Degree
  slots and pitches now come from the per-degree data DiArMaqAr supplies for each maqām and
  transposition, rather than from the tuning system's maqām-agnostic pitch-class list.
  Measured across all 48 tuning system + starting note combinations (64,848 maqām
  transpositions): **28.6% of them lost at least one degree before this fix; none do now.**
- **Heptatonic mode mapped the wrong notes to the white keys** on roughly a quarter of
  transpositions ([#1](https://github.com/Music-Intelligence-Lab/tanghim/issues/1)). White
  keys are now assigned by degree position rather than by each degree's note letter, which
  could collide and drop degrees. Degree collection also stops at the octave, fixing
  keyboards where pressing a higher white key sounded a lower pitch.
- **Pitches spelled with double accidentals** (`F##`, `B♭♭`, `E♭♭`, `C##`…) were not
  recognised and their degrees were skipped.
- **Slider display contradicted the tuning.** A degree moved to a different chromatic slot
  showed as a non-degree with a spurious "modified" marker, even though it was playing
  correctly.

### Changed

- Cached maqām data now stores the per-degree 12-EDO reference fields. Existing caches are
  refreshed automatically on first launch.

### Added

- **Hept from C** — a second heptatonic mode, next to Hept. Every maqām is mapped to the
  white keys starting from C, rather than from the maqām's own tonic key, so every maqām
  is fingered identically. The degrees and their pitches are unchanged; only the keyboard
  position differs. The two modes are mutually exclusive.
- **Update notifications.** Tanghīm checks GitHub for a newer release on startup and shows
  an **↑ Update** button in the status bar when one exists; clicking it opens the release
  page. This is separate from the existing **Updates** button, which tracks DiArMaqAr
  tuning-data versions — the two mean different things and both stay available.
- **Settings panel**, opened from the Settings button: turn automatic update checking off,
  check manually, and see the installed version. Automatic checking is on by default.
  Tanghīm never replaces its own files — a DAW holds the plug-in open while running — so
  updating is always "open the release page and run the installer".

## [0.9.0] - 2026-10-09

First public beta.

[Unreleased]: https://github.com/Music-Intelligence-Lab/tanghim/compare/v0.9.1...HEAD
[0.9.1]: https://github.com/Music-Intelligence-Lab/tanghim/compare/v0.9.0...v0.9.1
[0.9.0]: https://github.com/Music-Intelligence-Lab/tanghim/releases/tag/v0.9.0
