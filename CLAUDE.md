# Tanghīm

Cross-platform VST3/AU/CLAP MIDI tuning plugin using JUCE 8 (C++) with a React/TypeScript WebView UI. Fetches tuning data from the DiArMaqAr API and applies maqam-based microtuning via MTS-ESP (Transmitter), with MPE and 14-bit pitch bend output handled by the Receiver plugin.

**Two plugins in one project:**
- **Tanghim** (Transmitter) — main plugin with full UI, broadcasts tuning via MTS-ESP
- **Tanghim Receiver** — lightweight MIDI effect, reads MTS-ESP tuning and applies pitch bend/MPE to MIDI for non-MTS-ESP synths

## Build

```bash
# C++ plugin (debug — hot-reloads UI from Vite)
# IMPORTANT: Always build universal binary for Ableton compatibility (scanner runs x86_64)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Debug

# C++ plugin (release — embeds UI bundle from ui/dist/)
cd ui && npm run build && cd ..
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release

# React UI dev server (run alongside debug plugin)
cd ui && npm run dev

# Tests
cd build && ctest --output-on-failure

# Re-sign + install VST3s for DAW testing after each C++ rebuild
# IMPORTANT: Must rm old VST3 before copying — cp -R over existing corrupts the
# kernel's code signature cache, causing SIGKILL (Code Signature Invalid) in
# Ableton's Rosetta plugin scanner.
rm -rf ~/Library/Audio/Plug-Ins/VST3/Tanghim.vst3 ~/Library/Audio/Plug-Ins/VST3/Tanghim\ Receiver.vst3
cp -R "build/ArabicMaqamTuner_artefacts/Debug/VST3/Tanghim.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "build/ArabicMaqamTunerReceiver_artefacts/Debug/VST3/Tanghim Receiver.vst3" ~/Library/Audio/Plug-Ins/VST3/
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Tanghim.vst3
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Tanghim\ Receiver.vst3
```

Build timestamp auto-generated via `cmake/GenerateTimestamp.cmake`.

## Project Structure

```
source/
  PluginProcessor.h/.cpp    Main JUCE processor (owns API client, cache, engine)
  PluginEditor.h/.cpp       Hosts full-window WebBrowserComponent
  NativeBridge.h/.cpp       Registers all C++↔JS bridge functions via Options
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
ui/
  src/
    App.tsx                  Main app with state management
    constants.ts             Slider geometry constants (SLIDER_WIDTH_PX, etc.)
    hooks/useJuceBridge.ts   JUCE native function wrappers
    hooks/useVisibleSliderCount.ts  ResizeObserver hook for dynamic slider count
    types/index.ts           All TypeScript types
    components/              TuningSystemSelector, NoteSliderBank, MaqamSelector, CustomSelect, MaqamPresetBar, RangeScroller, etc.
tests/
  ApiResponseParserTest.cpp
  TuningEngineTest.cpp
  MidiProcessingTest.cpp
m4l/
  generate_patch.py          py2max script to regenerate .maxpat
  mts_midi_effect.js         Max js object for MIDI processing
  Tanghim Receiver.maxpat    Generated Max patch
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
Maqam detail API (`GET /maqamat/{id}?pitchClassDataType=all`) provides per-degree `ipnReferenceNoteName` and `solfege` — e.g. chromatic index 6 shows "F#" in Hijaz but "Gb" in Saba. `degreeIpnMap` + `degreeSolfegeMap` in tuning state JSON override slider labels when a maqam is selected. Cached per `systemId:startingNote:maqamId` in `~/Library/Tanghim/cache/maqam-detail/`. For transpositions: base detail provides `availableTranspositions` mapping, then fetch with `&transpositionId=`. → [diary 2026-02-22](diary/2026-02-22.md)

### Octave/Register Mapping (IMPORTANT)
Pitch classes are filtered by **MIDI note number**, NOT by the API's `octave` field:
- The API's `octave` field is **tonic-relative** — a G-based system has octave 1 spanning G2–F#3, not C3–B3
- MIDI note numbers are **absolute** (C3 = 48 regardless of tuning system)
- See `ApiResponseParser::buildVariantsPerSlot()` in `source/api/ApiResponseParser.cpp`

### PAO Name Preservation Across System Switches
When switching tuning systems, slider variant selection is matched by **PAO note name first** (rāst, segāh, etc.), with IPN-based default as fallback. This preserves musical intent even if a note maps to a different IPN position in the new system. See `PluginProcessor::loadTuningSystem()`.

## UI & Features

### Slider Bank Layout
- Fixed width 68px per slot (`SLOT_WIDTH_PX`), visible count via `ResizeObserver` in `useVisibleSliderCount`
- `centsOffset` per chromatic slot is **source of truth** for frequency tables; `selectedIndex` retained for snap markers/PAO display
- `BANK_LEFT_OFFSET_PX = 16` — aligns slider content with upper sections' 16px padding
- Viewport centering: `padding = Math.max(0, Math.floor((visibleCount - 13) / 2))` — 13 sliders = one maqam octave (tonic through octave above)
- Plugin window: 832–2400px wide, 620–4000px tall; min width = 16px + 12×68px = 832px
- `RangeScroller`: magnetic snap to C/G/A tick marks (within 1.1 MIDI notes), double-click centers on maqam octave
- Live MTS-ESP update: `setSlotCents` fire-and-forget on mousemove (RAF-throttled), `setSlotCentsFinalize` on mouseup
- No mouse/trackpad scrolling — panning only via RangeScroller
- Maqam modification tracking: degree highlights stay active, name shows ` *` suffix, modified thumbs turn cyan (#26c6da)
- → [diary 2026-02-21](diary/2026-02-21.md) (continuous tuning), [diary 2026-02-23b](diary/2026-02-23b.md) (design overhaul)

### UI Color Scheme
- **Red (accent)** = tuning system related: selector text, dropdown highlights, snap markers
- **Gold (#d4a843)** = maqam related: active preset border/background, degree highlights
- **Off-white (#b8b8c8)** = selector trigger text (all dropdowns)
- Text selection disabled globally (`-webkit-user-select: none` on `*`), inputs exempt
- Menu bar placeholder above tuning system selector (inside `top-bar-left`)

### Maqam Selector & Presets
- Two-dropdown: searchable maqam + variant/transposition (in `top-bar-left`)
- Transposition labels: `"PAOname / IPN / solfège (qarār)"`, sorted by `(octave, pitchClassIndex)`
- Presets: 8 displayed (4×2 grid), C++ has 16 slots. Store `degreeNames`, `centsOffsets`, optionally `tuningSystemId`+`startingNote`
- Modified presets: ` *` suffix, cyan thumbs, store tuning system for reload. Unmodified presets are portable
- Preset deactivation: click active preset → clears maqam, resets sliders, centers on C3
- Async system switch: `afterSystemSwitchRef` callback executed when `onTuningStateChanged` fires after system loads
- → [diary 2026-02-21](diary/2026-02-21.md), [diary 2026-02-23b](diary/2026-02-23b.md)

### Reference Frequency Control
- `referenceCentsOffset` (±700 cents, default 0). Formula: `f *= 2^(cents/1200)` in `TuningEngine::updateTuning()`
- APVTS: `ref_freq` param. **Resets to 0 on starting note/system change**
- UI: SVG arc knob + Hz input + cents display + ±100¢ semitone buttons
- Bridge: `setReferenceFreqCents` (fire-and-forget), `setReferenceFreqCentsFinalize`, gesture start/end
- → [diary 2026-02-22](diary/2026-02-22.md)

### Internal Reference Oscillator
- 16-voice polyphonic triangle wave, header-only (`TriangleOscillator.h`), -18 dBFS, 5ms attack, 100ms release
- "Osc" badge (coral #f09040). Additive — runs alongside MTS-ESP. No APVTS param (utility toggle)
- `oscillatorEnabled` (`std::atomic<bool>`), persisted in session state + `settings.json`. Tail length 0.15s
- Block-based rendering: `renderBlock()` iterates only active voices, writes via `getWritePointer()` + `FloatVectorOperations::copy()` for multi-channel
- → [diary 2026-02-22](diary/2026-02-22.md), [diary 2026-02-24](diary/2026-02-24.md) (performance optimization)

### Heptatonic Keyboard Mode
White keys play maqam degrees starting from the tonic's natural key. "Hept" badge (lavender #ab47bc). Inactive when no maqam.
- `heptMap[12]` (atomic ints): signed semitone deltas per chromatic position (typically ±1-2st)
- `activeHeptNotes[128]`: input→remapped note tracking for correct Note Off
- MIDI buffer rewritten via `midi.swapWith(remapped)`. MTS-ESP table remapped: `freq[N] = internalFreq[N+delta]`
- `rebuildHeptMap()` called from `applyMaqamDegrees()`, `loadTuningSystem()`, `clearCache()`
- Black keys: delta 0 (natural chromatic pitch). Black-key tonics: nearest white key below as start
- → [diary 2026-02-23](diary/2026-02-23.md)

### Per-Note Overrides
Per-MIDI-note variant overrides for different variants of same pitch class in different octaves. Blue thumb glow + accent IPN label. Stored in `ActiveTuningState::perNoteVariantOverrides[128]`.

### MIDI Activity Feedback
128-bit atomic bitmask (`uint32_t[4]`), editor timer (30fps) → `{ on: [...], off: [...] }` arrays to JS. `Set<number>` → gold thumb glow on exact played note.

### Native Status Bar & MIDI Drag Export
WKWebView is in a separate window hierarchy — native JUCE components can't overlap it. WebView trimmed by 26px bottom.
- Layout: version+timestamp (left); Preset MIDI Map Config label + device/channel dropdowns, MIDI drag button, Clear Cache, Updates (right)
- MIDI file: SMF Type 0, ASCII transliterated track name (DAWs use Mac Roman, not UTF-8), UTF-8 filename: `maqamname_(PAOname-IPN-solfege).mid`. Temp files in `~/Library/Tanghim/midi-export/`
- → [diary 2026-02-22](diary/2026-02-22.md), [diary 2026-02-23](diary/2026-02-23.md)

### Maqam List Caching
- `ApiDataCache`: lazy loading (scan filenames on startup, deserialize on first access), incremental saves to disk
- Cache directory: `~/Library/Tanghim/cache/`
- `fetchMaqamListIfNeeded()` runs after pitch classes loaded (pitch classes are critical path for slider display)

## JUCE 8 WebView Bridge

Native functions registered via `Options::withNativeFunction()` at construction time. `NativeBridge` must be constructed before `WebBrowserComponent`:

```
NativeBridge bridge(processor);            // 1. Create bridge
opts = bridge.applyTo(opts);               // 2. Register functions in Options
browser = make_unique<WebBrowserComponent>(opts);  // 3. Construct browser
```

- `NativeFunction` signature: `void(const Array<var>&, NativeFunctionCompletion)`
- Push to JS: `browser->emitEventIfBrowserIsVisible(eventId, varObject)`
- Debug: browser loads `http://localhost:5173` (Vite dev server)
- Release: browser serves from BinaryData (embedded `ui/dist/`)

### JS side (JUCE 8 module)

JUCE 8 injects a `./juce` ES module at runtime:
- `ui/src/hooks/juce.d.ts` — TypeScript declarations
- `ui/src/hooks/useJuceBridge.ts` — dynamic `import('./juce')` with try/catch for dev mode fallback
- Events: `window.__JUCE__.backend.addEventListener(eventId, callback)` — callback receives data directly (not wrapped)

## API

Base URL: `https://diarmaqar.netlify.app/api`

Key endpoints:
- `GET /tuning-systems` — list all systems
- `GET /tuning-systems/{id}/{startingNote}/pitch-classes?pitchClassDataType=all` — all pitch data
- `GET /tuning-systems/{id}/{startingNote}/maqamat?includeMaqamDegrees=true&includeTranspositions=true` — maqam list with degrees
- `GET /maqamat/{maqamId}?tuningSystem={id}&startingNote={note}&pitchClassDataType=all` — maqam detail with context-aware IPN
- `GET /maqamat/{maqamId}?tuningSystem={id}&startingNote={note}&pitchClassDataType=all&transpositionId={idName}` — transposed detail

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

Lightweight MIDI effect (`ArabicMaqamTunerReceiver`): MTS-ESP Client → pitch bend/MPE output.
- Two modes: MPE (ch 2-16, default 48st PB) and Mono PB (single channel, default 2st)
- APVTS: 3 params (mode, mpePbRange, monoPbRange). `IS_MIDI_EFFECT=TRUE`, `IS_SYNTH=FALSE`
- File-based registry: `~/Library/Tanghim/receivers/{uuid}.mpe|.monopb` — heartbeat 1Hz, stale >5s, cleanup >10s
- Transmitter scans at 2Hz; native count = `MTS_GetNumClients()` - (MPE + Mono PB count)
- Mono PB note stack: last-note priority legato recall
- PB wheel combining: `combined = clamp(microBend + (userPitchBend - 8192), 0, 16383)`
- → [diary 2026-02-18](diary/2026-02-18.md), [diary 2026-02-19](diary/2026-02-19.md), [diary 2026-02-22](diary/2026-02-22.md), [diary 2026-02-23b](diary/2026-02-23b.md)

## Max for Live Wrapper

Ableton doesn't support VST3 MIDI effects. Architecture: VST3 as data bridge (128 cents params) + Max `js` for MIDI processing + Max patch for routing.

**Key gotchas:**
- **JUCE VST3 bypass param at index 0** → APVTS params start at index 1. `cents_0` at **VST3 index 4** (3 control params + bypass)
- **`is_mpe: 1`** on patcher metadata — without it, Ableton normalizes to channel 1
- **Never reset PB on Note Off** — causes snap during release tail
- **midiparse outlet 5**: 7-bit (0-127), NOT 14-bit. Convert: `val << 7`
- Patch generated via py2max: `python3 m4l/generate_patch.py`
- Install: `~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim/`
- → [diary 2026-02-20](diary/2026-02-20.md) (comprehensive M4L architecture guide), [diary 2026-02-22](diary/2026-02-22.md) (PB combining)

## APVTS Parameters

| Parameter ID | Type | Range | Default | Purpose |
|---|---|---|---|---|
| `slot_0`–`slot_11` | Float | ±100.0 cents | 0.0 | Chromatic slot cents deviation |
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
- → [diary 2026-02-22](diary/2026-02-22.md) (sessions 29, 31), [diary 2026-02-25](diary/2026-02-25.md) (device selector fix)

## Plugin Naming

DAW-facing names must be pure ASCII (Ableton garbles UTF-8). No parentheses in PRODUCT_NAME (breaks JUCE CMake scripts).
- `"Tanghim"` / `"Tanghim Receiver"` (PRODUCT_NAME, ASCII)
- `"Tanghīm"` (WebView UI only — rendered by WebKit, not DAW)
- CMake targets: `ArabicMaqamTuner` / `ArabicMaqamTunerReceiver`

## Conventions

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

### Ableton Live Debugging
- Plugin scanner log: `~/Library/Preferences/Ableton/Live 11.3.43/PluginScanner.txt`
- Crash reports: `~/Library/Application Support/Ableton/Live Reports/` (check first)
- System crashes: `/Library/Logs/DiagnosticReports/Ableton Plugin Scanner-*.ips`
- Scanner runs x86_64 under Rosetta on Apple Silicon

### Known Issue: WKWebView + Ableton Computer MIDI Keyboard
WKContentView retains first responder after text input blur → timing delays in Ableton's CMK. External MIDI unaffected. Six ObjC swizzling approaches tried, none resolved. Workaround: click outside plugin window. → [diary 2026-02-20](diary/2026-02-20.md)

### Release Build (BinaryData Resource Provider)
JUCE BinaryData **removes** dashes from filenames (not replaces with `_`). Match against `BinaryData::originalFilenames[]`, strip directory prefix before lookup. → [diary 2026-02-20](diary/2026-02-20.md)
