# Tanghīm

Cross-platform VST3/AU/CLAP MIDI tuning plugin using JUCE 8 (C++) with a native JUCE editor. Fetches tuning data from the DiArMaqAr API and applies maqam-based microtuning via MTS-ESP (Transmitter), with MPE and 14-bit pitch bend output handled by the Receiver plugin.

**Two plugins in one project:**
- **Tanghim** (Transmitter) — main plugin with full UI, broadcasts tuning via MTS-ESP
- **Tanghim Receiver** — lightweight MIDI effect, reads MTS-ESP tuning and applies pitch bend/MPE to MIDI for non-MTS-ESP synths

## Build

```bash
# C++ plugin (debug)
# IMPORTANT: Always build universal binary for Ableton compatibility (scanner runs x86_64)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Debug

# C++ plugin (release)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release

# Tests
cd build && ctest --output-on-failure

# Re-sign + install VST3s for DAW testing after each C++ rebuild
# IMPORTANT: Must rm old VST3 before copying — cp -R over existing corrupts the
# kernel's code signature cache, causing SIGKILL (Code Signature Invalid) in
# Ableton's Rosetta plugin scanner.
rm -rf ~/Library/Audio/Plug-Ins/VST3/Tanghim.vst3 ~/Library/Audio/Plug-Ins/VST3/Tanghim\ Receiver.vst3
cp -R "build/Tanghim_artefacts/Debug/VST3/Tanghim.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "build/TanghimReceiver_artefacts/Debug/VST3/Tanghim Receiver.vst3" ~/Library/Audio/Plug-Ins/VST3/
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Tanghim.vst3
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Tanghim\ Receiver.vst3
```

Build timestamp auto-generated via `cmake/GenerateTimestamp.cmake`.

## CI/CD & Releases

GitHub Actions workflow (`.github/workflows/release.yml`) builds and publishes releases automatically on tag push. Auto-deletes all previous releases — only the latest is ever visible.

```bash
# New release (bump version in CMakeLists.txt line 2 first)
git tag v0.1.2
git push origin main v0.1.2

# Retrigger same version (e.g. after a fix, no version bump needed)
git push origin --delete v0.1.2
git tag -d v0.1.2
git tag v0.1.2
git push origin v0.1.2
```

**What it does:**
- Builds on macOS (universal arm64+x86_64), Windows, and Linux
- Builds C++ plugins (no Node.js/npm needed — pure C++ UI)
- Runs tests on all platforms
- Codesigns macOS bundles (ad-hoc)
- Packages: VST3 + AU + CLAP (macOS), VST3 + CLAP (Windows/Linux), M4L (cross-platform)
- Creates GitHub Release with all artifacts and install instructions

**Formats per platform:**
- macOS: VST3, AU, CLAP + M4L Receiver
- Windows: VST3, CLAP (no AU)
- Linux: VST3, CLAP (no AU)

**No manual zip/upload needed** — just push a tag.

## Project Structure

```
source/
  PluginProcessor.h/.cpp    Main JUCE processor (owns API client, cache, engine)
  editor/
    TanghimNativeEditor.h/.cpp  Full native editor (replaces WebView), timer-based state sync
    TanghimTheme.h              Shared theme colours and layout constants
    NoteSliderComponent.h/.cpp  Individual tuning slider with IPN/solfege/PAO labels
    NoteSliderBankComponent.h/.cpp  Scrollable bank of 128 note sliders
    ReferenceFreqControl.h/.cpp     Arc knob + Hz input + cents display + semitone buttons
    MaqamSelectorComponent.h/.cpp   Maqam + transposition dropdowns with SearchablePopup
    MaqamPresetBar.h/.cpp           8-slot preset grid (4x2) with MIDI learn
    RangeScrollerComponent.h/.cpp   MIDI range scroller with magnetic snap
    OutputBadges.h/.cpp             MTS-ESP/Osc/Hept/MPE/MonoPB status badges
    SearchablePopup.h/.cpp          Reusable searchable dropdown popup
  model/
    PitchClass.h             Single pitch with IPN reference logic
    TuningSystem.h           Tuning system metadata
    TwelvePitchClassSet.h    12-note set with compatible maqamat (unused, kept for reference)
    ActiveTuningState.h      Current slider positions → 128-note frequency table
    MaqamPreset.h            Stored preset (maqam + 12 slider positions)
  api/
    DiArMaqArClient.h/.cpp   Async HTTP client (background thread)
    ApiDataCache.h/.cpp      Disk-backed JSON cache (lazy load, incremental save)
    ApiResponseParser.h/.cpp JSON → model structs
    DataUpdateChecker.h/.cpp Version-based update detection
  engine/
    TuningEngine.h/.cpp      Builds 128-note tables, broadcasts via MTS-ESP
    MtsEspTransmitter.h/.cpp MTS-ESP transmitter wrapper (libMTSMaster.h)
    MpePitchBendProcessor.h/.cpp   MPE channel allocation + per-note pitch bend (shared with Receiver)
    MonoPitchBendProcessor.h/.cpp  14-bit mono pitch bend with note stack (shared with Receiver)
    MidiFileGenerator.h/.cpp SMF Type 0 generator for maqam scale export
    TriangleOscillator.h     Header-only polyphonic triangle wave reference oscillator
  receiver/
    ReceiverProcessor.h/.cpp MTS-ESP client + pitch bend MIDI effect processor
    ReceiverEditor.h/.cpp    Minimal JUCE-native editor (mode, PB range, status)
    ReceiverRegistry.h       File-based IPC for receiver type discovery (header-only)
tests/
  ApiResponseParserTest.cpp
  TuningEngineTest.cpp
  MidiProcessingTest.cpp
m4l/
  generate_patch.py          py2max script to regenerate .maxpat
  mts_midi_effect.js         Max js object for MIDI processing
  Tanghim Receiver.maxpat    Generated Max patch
  Tanghim Receiver.amxd      Compiled Max for Live device
libs/
  JUCE/                      Git submodule
  clap-juce-extensions/      Git submodule
  MTS-ESP/                   Git submodule
```

## Critical Musicological Logic

### IPN Reference Assignment
IPN references respect Arabic maqam theory. Microtonal modifiers indicate what a pitch is a **variant OF**, not proximity to 12-EDO:
- `E-b3` (E half-flat / segāh) → variant of **E**, not Eb
- `D-#3` (D half-sharp / nīm zīrgūleh) → variant of **D#**, not D
- `englishName` field from the API is the primary source of truth
- Chromatic order uses sharps internally: `C, C#, D, D#, E, F, F#, G, G#, A, A#, B`
- Logic implemented in `PitchClass::ipnReferenceFromEnglishName()`

### Context-Aware IPN Labels & Solfege
Maqam list endpoint (`GET /tuning-systems/{id}/{note}/maqamat?includeMaqamDegrees=true&includeTranspositions=true&includeDegreeDetails=true`) provides per-degree `englishName` and `solfege` inline for all maqamat and all transpositions — e.g. chromatic index 6 shows "F#" in Hijaz but "Gb" in Saba. `degreeIpnMap` + `degreeSolfegeMap` in tuning state JSON override slider labels when a maqam is selected. IPN refs are derived from `englishName` (strip octave digits + microtonal modifiers) and populated synchronously in `applyDegreeIpnAndSolfege()` before `rebuildHeptMap()` — no async fetch needed. → [diary 2026-02-22](diary/2026-02-22.md)

### Octave/Register Mapping (IMPORTANT)
Pitch classes are filtered by **MIDI note number**, NOT by the API's `octave` field:
- The API's `octave` field is **tonic-relative** — a G-based system has octave 1 spanning G2–F#3, not C3–B3
- MIDI note numbers are **absolute** (C3 = 48 regardless of tuning system)
- See `ApiResponseParser::buildVariantsPerSlot()` in `source/api/ApiResponseParser.cpp`

### PAO Name Preservation Across System Switches
When switching tuning systems, slider variant selection is matched by **PAO note name first** (rāst, segāh, etc.), with IPN-based default as fallback. This preserves musical intent even if a note maps to a different IPN position in the new system. See `PluginProcessor::loadTuningSystem()`.

## UI & Features

### Slider Bank Layout
- Fixed width 68px per slot (`Theme::kSlotWidthPx`), visible count computed from component width
- `centsOffset` per chromatic slot is **source of truth** for frequency tables; `selectedIndex` retained for snap markers/PAO display
- `Theme::kBankLeftOffsetPx = 16` — aligns slider content with upper sections' 16px padding
- Viewport centering: `padding = max(0, (visibleCount - 13) / 2)` — 13 sliders = one maqam octave (tonic through octave above)
- Plugin window: 832–2400px wide, 620–4000px tall; min width = 16px + 12×68px = 832px
- `RangeScrollerComponent`: magnetic snap to C/G/A tick marks (within 1.1 MIDI notes), double-click centers on maqam octave
- Live MTS-ESP update: slider drag calls processor directly, finalize on mouse up
- No mouse/trackpad scrolling — panning only via RangeScroller
- Maqam modification tracking: degree highlights stay active, name shows ` *` suffix, modified thumbs turn cyan (#26c6da)

### UI Color Scheme
- **Red (accent)** = tuning system related: selector text, dropdown highlights, snap markers, MIDI preset dropdowns
- **Gold (#d4a843)** = maqam related: active preset border/background, degree highlights, maqam selector dropdown highlights/borders
- **Off-white (#b8b8c8)** = selector trigger text (all dropdowns)
- **Badge colors**: MTS-ESP `#26a69a` (teal), Osc `#ffa726` (amber), Hept `#d050e0` (magenta), MPE `#64b5f6` (blue), Mono PB `#66bb6a` (green). Osc/Hept fill background at 0.2 opacity when active
- Theme colours and layout constants in `TanghimTheme.h`, matching original CSS variables
- `TanghimLookAndFeel` in `TanghimTheme.h` — custom LookAndFeel for the entire editor

### Maqam Selector & Presets
- `MaqamSelectorComponent`: two `SearchablePopup` dropdowns — maqam + transposition
- Transposition labels: `"PAOname / IPN / solfège (qarār)"`, sorted by `(octave, pitchClassIndex)`
- `MaqamPresetBar`: 8 displayed (4×2 grid), C++ has 16 slots. Store `degreeNames`, `centsOffsets`, optionally `tuningSystemId`+`startingNote`
- Modified presets: ` *` suffix, cyan thumbs, store tuning system for reload. Unmodified presets are portable
- Preset deactivation: click active preset → clears maqam, resets sliders, centers on C3
- Auto-preset activation: selecting a maqam+tonic from dropdown auto-activates matching unmodified preset (exact `maqamId` + `setIndex`, excludes modified)

### Reference Frequency Control
- `referenceCentsOffset` (±700 cents, default 0). Formula: `f *= 2^(cents/1200)` in `TuningEngine::updateTuning()`
- APVTS: `ref_freq` param. **Persists across system/note switches**
- `ReferenceFreqControl`: arc knob + Hz input + cents display + ±100¢ semitone buttons + transposition label
- Transposition indicator: always shows "X → Y" style IPN shift (e.g., "C ↗ C#", "G ↘ F#−"). Uses ↗/↘/→ arrows, +/− suffix for non-clean semitones
- Hz input supports up/down arrow keys for ±1 Hz nudges

### Slider Bank Visual Indicators
- Piano key indicator bars below cents: white key `rgba(255,255,255,0.6)`, black key `rgba(255,255,255,0.15)`, 3px height
- In hept mode, key bars reflect remapped layout (degrees = white key, others = standard). Muted positions get transparent bars
- Hept muted sliders: unreachable positions dimmed to 20% opacity, pointer-events disabled
- Hept IPN remapping: "Eb3→E" style labels when white key differs from degree position. `NoteSliderBankComponent` mirrors C++ `rebuildHeptMap()` logic

### Internal Reference Oscillator
- 16-voice polyphonic triangle wave, header-only (`TriangleOscillator.h`), -18 dBFS, 5ms attack, 100ms release
- "Osc" badge (amber #ffa726). Additive — runs alongside MTS-ESP. No APVTS param (utility toggle)
- `oscillatorEnabled` (`std::atomic<bool>`), persisted in session state + `settings.json`. Tail length 0.15s
- All internal arithmetic in `float` (not double) — 2× cache throughput, 4-wide SIMD, no per-sample casts
- Segmented block rendering: `renderVoice()` pre-computes envelope transition sample counts, renders tight loops for attack/sustain/release without per-sample branching. Sustain loop has no envelope multiply
- Scratch buffer (`alignas(16) float[8192]`) — voice renders to scratch, then gain+accumulate in separate auto-vectorizable loop. Writes via `getWritePointer()` + `FloatVectorOperations::copy()` for multi-channel
- → [diary 2026-02-22](diary/2026-02-22.md), [diary 2026-02-24](diary/2026-02-24.md) (performance optimization), [diary 2026-02-24c](diary/2026-02-24c.md) (float + segmented rendering)

### Heptatonic Keyboard Mode
White keys play maqam degrees starting from the tonic's natural key. "Hept" badge (magenta #d050e0). Inactive when no maqam.
- `heptMap[12]` (atomic ints): signed semitone deltas per chromatic position (typically ±1-2st)
- `activeHeptNotes[128]`: input→remapped note tracking for correct Note Off
- MIDI buffer rewritten via `midi.swapWith(remapped)`. MTS-ESP table remapped: `freq[N] = internalFreq[N+delta]`
- `rebuildHeptMap()` called from `applyMaqamDegrees()`, `loadTuningSystem()`, `clearCache()`
- Black keys: delta 0 (natural chromatic pitch). Black-key tonics: nearest white key below as start
- → [diary 2026-02-23](diary/2026-02-23.md)

### Per-Note Overrides
Per-MIDI-note variant overrides for different variants of same pitch class in different octaves. Blue thumb glow + accent IPN label. Stored in `ActiveTuningState::perNoteVariantOverrides[128]`.

### MIDI Activity Feedback
128-bit atomic bitmask (`uint32_t[4]`), editor timer (30fps) polls and updates slider gold thumb glow on exact played note.

### Status Bar & MIDI Drag Export
- 26px bottom bar. Layout: version+timestamp (left); Preset MIDI Map Config label + device/channel dropdowns, MIDI drag button, Clear Cache, Updates (right)
- MIDI drag button: always visible. Grey/inactive when no maqam selected, gold/active when maqam is selected
- MIDI file: SMF Type 0, ASCII transliterated track name (DAWs use Mac Roman, not UTF-8), UTF-8 filename: `maqamname_(PAOname-IPN-solfege).mid`. Temp files in `~/Library/Tanghim/midi-export/`
- **Program change in MIDI export**: If a preset is active when exporting, a Program Change message (PC 0-7) is embedded before the notes so the preset auto-activates when the clip plays back through the plugin
- → [diary 2026-02-22](diary/2026-02-22.md), [diary 2026-02-23](diary/2026-02-23.md)

### Program Change Preset Switching
- Always-on: `processBlock` listens for MIDI Program Change messages (PC 0-7 → presets 0-7, PC 8+ ignored)
- Uses the same `pendingMidiPreset` atomic as MIDI Learn — consumed by editor timer at 30Hz → `applyPreset()`
- Program change messages are consumed (not passed through to output)
- Works via DAW MIDI routing (unlike MIDI Learn which uses a dedicated device)

### Save/Load State Files (.tanghim)
User-managed state persistence via human-readable JSON files with `.tanghim` extension. Complements DAW session and disk preferences.
- `buildStateJson()` / `restoreStateFromJson()` on `PluginProcessor` (mirrors `getStateInformation`/`setStateInformation` minus APVTS automation)
- Native Save/Load dialogs via async `juce::FileChooser` (stored as `TanghimNativeEditor::fileChooser` member — must outlive dialog)
- **Modification detection on load**: callback compares `slot.centsOffset` vs `expectedVariant.midiCentsDeviation` to populate `modifiedSlots` (cyan thumbs + asterisk)
- **Display info capture**: maqam display strings saved as locals before `loadTuningSystem` (which clears them), restored in completion lambda
- → [diary 2026-02-25](diary/2026-02-25.md)

### Maqam List Caching
- `ApiDataCache`: lazy loading (scan filenames on startup, deserialize on first access), incremental saves to disk
- Cache directory: `~/Library/Tanghim/cache/`
- `fetchMaqamListIfNeeded()` runs before `notifyTuningChanged()` in `doLoad()` so preset compatibility checks have data on first render
- Enriched degree data (`ascendingEnglishNames`, `ascendingSolfeges`) cached alongside PAO names in maqam list entries
- No separate maqam detail cache — all IPN/solfège data comes from the maqam list
- **Cache key ↔ filename mapping**: Keys use `:` separator (`systemId:startingNote`), filenames use `_` (`systemId_startingNote.json`). NEVER use `replaceCharacter('_', ':')` or vice versa to convert — system IDs and maqam IDs contain underscores. Use `lastIndexOfChar('_')` for 2-part keys. → [diary 2026-02-27](diary/2026-02-27.md)

## Native Editor Architecture

`TanghimNativeEditor` (in `source/editor/`) is the full editor, composed of native JUCE components. No WebView, no JavaScript, no Node.js dependency.

- **State sync**: 30Hz timer polls processor for changes via `notifyTuningChanged()` dirty flag, calls `syncFullState()` → `computeDerivedState()`
- **Derived state**: `maqamDegreeIndices`, `degreePaoNameMap`, `maqamTonicMidi`, `modifiedSlots`, `isMaqamModified` — computed in editor from processor tuning state
- **Component hierarchy**: TuningSystemSelector, MaqamSelector, ReferenceFreqControl, OutputBadges, NoteSliderBank, MaqamPresetBar, RangeScroller, StatusBar
- **SearchablePopup**: reusable filterable dropdown used by tuning system, starting note, maqam, and transposition selectors
- **Theme**: all colours and layout constants in `TanghimTheme.h`, `TanghimLookAndFeel` for consistent styling

## API

Base URL: `https://diarmaqar.netlify.app/api`

Key endpoints:
- `GET /tuning-systems` — list all systems
- `GET /tuning-systems/{id}/{startingNote}/pitch-classes?pitchClassDataType=all` — all pitch data
- `GET /tuning-systems/{id}/{startingNote}/maqamat?includeMaqamDegrees=true&includeTranspositions=true&includeDegreeDetails=true` — maqam list with enriched degrees (englishName + solfege per degree, for all maqamat + transpositions)

### Response structure

**`/tuning-systems`**: `{ count, data: [{ tuningSystem: { id, idName, displayName, version, year }, startingNotes: { idNames: [...], displayNames: [...] }, stats: {...} }] }`

**`/pitch-classes`**: `{ tuningSystem: {...}, pitchClasses: [...] }` — field is `midiNotePlusCentsDeviation` (not `midiNoteDeviation`), `ipnReferenceNoteName` and `solfege` provided by API

MIDI deviation format: `"48 -5.9"` = MIDI note 48, -5.9 cents from 12-EDO.

## MTS-ESP API

- `MTS_FilterNote(bool doFilter, char midinote, signed char midichannel)` — 3 args
- `MTS_ClearNoteFilter()` — 0 args
- `MTS_SetNoteTunings(const double* freqs)` — 128-note frequency table
- **IPC recovery**: stale shared memory → `MTS_Reinitialize()` then `MTS_RegisterMaster()`
- **Terminology**: Transmitter/Receiver (not Master/Client). C API names unchanged
- JSON keys: `isMtsTransmitter`, `mtsReceivers`, `mtsNativeCount`, `mpeCount`, `monoPbCount`
- Lightweight `mtsStatusChanged` event (2Hz poll, separate from full `tuningStateChanged`)

## Receiver Plugin

Lightweight MIDI effect (`TanghimReceiver`): MTS-ESP Client → pitch bend/MPE output.
- Two modes: MPE (ch 2-16, default 48st PB) and Mono PB (single channel, default 2st)
- APVTS: 3 params (mode, mpePbRange, monoPbRange). `IS_MIDI_EFFECT=TRUE`, `IS_SYNTH=FALSE`
- File-based registry: `~/Library/Tanghim/receivers/{uuid}.mpe|.monopb` — heartbeat 1Hz, stale >5s, cleanup >10s
- Transmitter scans at 2Hz; native count = `MTS_GetNumClients()` - (MPE + Mono PB count)
- Mono PB note stack: last-note priority legato recall
- PB wheel combining: `combined = clamp(microBend + (userPitchBend - 8192), 0, 16383)`
- → [diary 2026-02-18](diary/2026-02-18.md), [diary 2026-02-19](diary/2026-02-19.md), [diary 2026-02-22](diary/2026-02-22.md), [diary 2026-02-23b](diary/2026-02-23b.md)

## Max for Live Wrapper

Ableton doesn't support VST3 MIDI effects. Architecture: VST3 as data bridge (128 cents params) + Max `js` for MIDI processing + Max patch for routing.

**Frozen .amxd (single-file distribution):**
- The `.amxd` embeds `mts_midi_effect.js` inside the binary — users install one file, no external JS needed
- Format: `ampf` header + `mmmmmeta` (value 4 = frozen MIDI effect) + `ptch` section containing `mx@c` header + concatenated files + `dlst` directory
- Meta tag prefix encodes device type: `mmmmm` = MIDI effect, `iiiii` = instrument, `aaaaa` = audio effect
- Frozen meta values: `4` for MIDI effects, `7` for audio effects/instruments
- `dependency_cache` in patch JSON lists embedded files; `project.contents.code` registers JS
- `dlst` directory: `dire` entries with `type`/`fnam`/`sz32`/`of32`/`flag` sub-fields (all big-endian). Main patch has `flag: 17`, dependencies `flag: 0`
- Reverse-engineered from Ableton factory frozen devices (LFO.amxd, Max MIDI Receiver.amxd)

**Key gotchas:**
- **JUCE VST3 bypass param at index 0** → APVTS params start at index 1. `cents_0` at **VST3 index 4** (3 control params + bypass)
- **`is_mpe: 1`** on patcher metadata — without it, Ableton normalizes to channel 1
- **Never reset PB on Note Off** — causes snap during release tail
- **midiparse outlet 5**: 7-bit (0-127), NOT 14-bit. Convert: `val << 7`
- **`vst~` ignoreclick**: The `vst~` object must have `ignoreclick 1` — without it, clicking anywhere on the M4L device (even blank background) steals keyboard focus from Ableton, disabling computer keyboard MIDI input. Native Ableton devices don't have this issue because they use Ableton's own UI framework, not Max's
- **JS default initialization**: pattr `@default` may not output on first load (no pattrstorage state). Explicit message box sends `set_mode 0, set_mpe_bend_range 48, set_mono_bend_range 2` to JS on loadbang delay
- Patch generated via py2max: `python3 m4l/generate_patch.py`
- Install: `~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim/`
- → [diary 2026-02-20](diary/2026-02-20.md) (comprehensive M4L architecture guide), [diary 2026-02-22](diary/2026-02-22.md) (PB combining)

## APVTS Parameters

| Parameter ID | Type | Range | Default | Purpose |
|---|---|---|---|---|
| `slot_0`–`slot_11` | Float | ±150.0 cents | 0.0 | Chromatic slot cents deviation |
| `ref_freq` | Float | ±700.0 cents | 0.0 | Reference frequency offset |
| `preset` | Choice | "None", "1"–"16" | 0 | Active preset index |

Bidirectional sync with UI. Gesture marking (`beginSliderGesture`/`endSliderGesture`) for DAW automation. Guard flag `updatingParamsFromCode` prevents feedback loops. Per-note overrides are UI-only (not APVTS).

## MIDI Learn: Per-Preset Note Mapping

MIDI notes mapped to presets for live maqam switching. Uses **dedicated MIDI input device** (bypasses DAW routing for reliable triggering).

- **Status bar controls**: MIDI Input device dropdown + Channel filter (1-16 or All)
- **Interaction**: Shift+click preset → learn mode → play note → mapped. Click badge → clear
- **State**: `midiPresetNotes[16]` (atomic ints), `midiPresetChannel`, `pendingMidiPreset` (consumed by 30Hz editor timer → `applyPreset()` + `emitTuningStateChanged()`)
- **Channel filtering**: `processBlock()` skips MIDI on preset channel (prevents oscillator double-trigger)
- **Device persistence**: `setMidiPresetDevice()` always close+reopen (no idempotency check — stale pointers blocked reconnection). Stores both `midiPresetDeviceName` (for UI) and `midiPresetDeviceId` (OS-unique identifier for reliable matching). `setStateInformation()` only overrides device if non-empty (protects settings.json from empty session values)
- **Stale connection detection**: `recheckMidiPresetDevice()` called on `midiDevicesChanged` — verifies device identifier/name still in available list, closes stale `MidiInput` if device disappeared. Timer then retries via `setMidiPresetDevice()` (tries identifier match first, falls back to name)
- **Persistence**: `~/Library/Tanghim/settings.json` — not APVTS (user preference, not automatable)
- → [diary 2026-02-22](diary/2026-02-22.md) (sessions 29, 31), [diary 2026-02-23c](diary/2026-02-23c.md) (device selector fix)

## Plugin Naming

DAW-facing names must be pure ASCII (Ableton garbles UTF-8). No parentheses in PRODUCT_NAME (breaks JUCE CMake scripts).
- `"Tanghim"` / `"Tanghim Receiver"` (PRODUCT_NAME, ASCII)
- `"Tanghīm"` (editor title bar only — rendered by JUCE, not DAW)
- CMake targets: `Tanghim` / `TanghimReceiver`

## Conventions

- **No fallbacks or graceful degradation**: Never add fallback logic, last-resort behaviors, or silent substitution. If primary logic fails or data is missing, skip/omit — do not degrade to an alternative. This applies to data resolution (e.g., no `ipnReference` fallback when `englishName` parsing fails), UI behavior (e.g., no mid-word text breaks as fallback), and all other code paths
- **Tuning system ≡ tuning system + starting note**: Switching the starting note is functionally identical to switching the tuning system — both trigger `loadTuningSystem()`, reload pitch classes, refresh the maqam list, and require the same preset compatibility checks, state clearing, and UI updates. Any behaviour implemented for tuning system switches MUST also work for starting note switches. They share the same code path (`loadTuningSystem()` in `PluginProcessor`).
- **Lifetime guard**: `callAsync` lambdas capture `weak_ptr<atomic<bool>>`, check `isAlive(weak)`. `alive` declared BEFORE `apiClient` so it outlives background thread
- **Background cache preloading**: `apiClient.runOnThread()` for JSON deserialization, `callAsync` back to message thread
- **MidiBufferIterator**: no `operator->`. Use `(*it).getMessage()` or range-for `meta.getMessage()`
- **Test executables**: one per file, `JUCE_STANDALONE_APPLICATION=0`
- **Transmitter**: `IS_SYNTH=TRUE`, `IS_MIDI_EFFECT=FALSE`, `VST3_CATEGORIES "Instrument" "Tools"`. Audio bus: input+output stereo, `audio.clear()` in processBlock
- **Receiver**: `IS_SYNTH=FALSE`, `IS_MIDI_EFFECT=TRUE`, `VST3_CATEGORIES "Tools"`
- **Codesign**: Must `rm -rf` old VST3 before `cp -R` (overwriting corrupts kernel signature cache → SIGKILL under Rosetta), then `codesign --force --deep --sign -`
- **DynamicObject**: Never pass `unique_ptr::get()` to `juce::var` (double-free). Use `new` or `release()`
- **Ableton MIDI routing**: Merges all MIDI to ch1 between tracks. MTS-ESP recommended for Ableton
- **processBlock idle guard**: Early return when `midi.isEmpty() && (!oscOn || !oscillator.hasActiveVoices())`. Wrap MIDI iteration in `if (!midi.isEmpty())`
- **ScopedNoDenormals**: Always at top of processBlock — prevents denormalized float CPU spikes during envelope release tails
- **Audio buffer writes**: Use `getWritePointer()` + `FloatVectorOperations::copy()` instead of `addSample()` (per-call bounds check overhead)
- **Frequency table access from audio thread**: Use `snapshotFrequencyTable()` (one lock) instead of per-note `getFrequencyForMidiNote()` (one lock each)
- **MIDI device enumeration**: Event-driven via `MidiDeviceListConnection::make()` (cross-platform), not polling. Atomic flag checked in editor timer
- **Filesystem I/O in editor timer**: Rate-limited to ~0.2Hz (ReceiverRegistry::scan, stale cleanup). Cheap shared-memory reads (MTS-ESP) stay at 2Hz
- **APVTS automation fast paths**: DAW automation / MIDI CC mapping calls `parameterChanged` at hundreds of Hz. Use fast tuning update (e.g. `updateReferenceOffset` for ref_freq) instead of full `updateTuningAndBroadcast()`, and set a dirty flag instead of `notifyTuningChanged()`. Editor timer picks up dirty flags at 30Hz

### Ableton Live Debugging
- Plugin scanner log: `~/Library/Preferences/Ableton/Live 11.3.43/PluginScanner.txt`
- Crash reports: `~/Library/Application Support/Ableton/Live Reports/` (check first)
- System crashes: `/Library/Logs/DiagnosticReports/Ableton Plugin Scanner-*.ips`
- Scanner runs x86_64 under Rosetta on Apple Silicon

