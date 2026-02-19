# Arabic Maqam Tuner

Cross-platform VST3/AU/CLAP microtuning plugin using JUCE 8 (C++) with a React/TypeScript WebView UI. Fetches tuning data from the DiArMaqAr API and applies maqam-based microtuning via MTS-ESP, MPE, and 14-bit monophonic pitch bend.

**Two plugins in one project:**
- **Transmitter** (Arabic Maqam Tuner) — main plugin with full UI, broadcasts tuning via MTS-ESP
- **Receiver** (Arabic Maqam Tuner Receiver) — lightweight MIDI effect, reads MTS-ESP tuning and applies pitch bend/MPE to MIDI for non-MTS-ESP synths

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

# Copy VST3s to system plugin dir for DAW testing after each C++ rebuild
cp -R "build/ArabicMaqamTuner_artefacts/Debug/VST3/Arabic Maqam Tuner.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "build/ArabicMaqamTunerReceiver_artefacts/Debug/VST3/Arabic Maqam Tuner Receiver.vst3" ~/Library/Audio/Plug-Ins/VST3/
```

## Project Structure

```
source/
  PluginProcessor.h/.cpp    Main JUCE processor (owns API client, cache, engine)
  PluginEditor.h/.cpp       Hosts full-window WebBrowserComponent
  NativeBridge.h/.cpp       Registers all C++↔JS bridge functions via Options
  model/
    PitchClass.h             Single pitch with IPN reference logic
    TuningSystem.h           Tuning system metadata
    TwelvePitchClassSet.h    12-note set with compatible maqamat
    ActiveTuningState.h      Current slider positions → 128-note frequency table
    MaqamPreset.h            Stored preset (maqam + 12 slider positions)
  api/
    DiArMaqArClient.h/.cpp   Async HTTP client (background thread)
    ApiDataCache.h/.cpp      Disk-backed JSON cache (lazy load, incremental save)
    ApiResponseParser.h/.cpp JSON → model structs
    DataUpdateChecker.h/.cpp Version-based update detection
  engine/
    TuningEngine.h/.cpp      Builds 128-note tables, routes to output mode
    MtsEspTransmitter.h/.cpp MTS-ESP transmitter wrapper (libMTSMaster.h)
    MpePitchBendProcessor.h/.cpp   MPE channel allocation + per-note pitch bend (shared with Receiver)
    MonoPitchBendProcessor.h/.cpp  14-bit mono pitch bend (shared with Receiver)
  receiver/
    ReceiverProcessor.h/.cpp MTS-ESP client + pitch bend MIDI effect processor
    ReceiverEditor.h/.cpp    Minimal JUCE-native editor (mode, PB range, status)
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
  Arabic Maqam Tuner Receiver.maxpat  Generated Max patch
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

### Octave/Register Mapping (IMPORTANT)
Pitch classes are filtered by **MIDI note number**, NOT by the API's `octave` field:
- The API's `octave` field is **tonic-relative** — a G-based system has octave 1 spanning G2–F#3, not C3–B3
- MIDI note numbers are **absolute** (C3 = 48 regardless of tuning system)
- See `ApiResponseParser::buildVariantsPerSlot()` in `source/api/ApiResponseParser.cpp`

### PAO Name Preservation Across System Switches
When switching tuning systems, slider variant selection is matched by **PAO note name first** (rāst, segāh, etc.), with IPN-based default as fallback:
1. Collect all 12 previously selected PAO names
2. Rebuild slots from new system (default to first variant per IPN slot)
3. For each slot, if any variant's PAO name was previously selected, select it
4. This preserves musical intent even if a note maps to a different IPN position in the new system
- See `PluginProcessor::loadTuningSystem()` in `source/PluginProcessor.cpp`

### PAO Note Name Display
- PAO names (rāst, dūgāh, segāh, nawā, ḥusaynī, etc.) are displayed below the slider with Unicode diacritics
- Names can wrap to two lines at word boundaries only (`word-break: keep-all`); long names break at `/` using `<wbr/>`
- Cents deviation is shown above the note name

### Slider Bank Layout
- Sliders have a **fixed width** of 68px (`ui/src/constants.ts: SLIDER_WIDTH_PX`)
- The number of visible sliders is computed dynamically via `ResizeObserver` in `useVisibleSliderCount` hook
- Widening the plugin window reveals more sliders; narrowing hides them — no CSS scrolling, virtual render only
- `RangeScroller` pans one MIDI note at a time (smooth scrolling), with tick marks at every C, G, A
- Mouse wheel on the slider bank also scrolls the range
- Plugin window: 884–2400px wide, 590–900px tall (`PluginEditor.cpp: setResizeLimits`)
- Min width = 13 sliders × 68px (full octave including tonic octave above)
- Min height = maqam dropdown (max-height 480px) lines up flush with status bar
- Default window: 900×590, default start MIDI: 48 (C3), default 13 visible sliders

### Per-Note Overrides
- Shift+click/drag on a slider creates a per-MIDI-note variant override (different from the chromatic slot default)
- Override indicator: blue thumb glow + accent-coloured IPN label
- Stored in `ActiveTuningState::perNoteVariantOverrides[128]`

### MIDI Activity Feedback
- C++ tracks per-note activity via 128-bit atomic bitmask (`uint32_t[4]` for on/off)
- Editor timer (30fps) exchanges bitmasks and sends arrays of MIDI note numbers to JS: `{ on: [60, 64], off: [48] }`
- JS maintains a `Set<number>` of active MIDI notes — highlights only the exact note being played (gold thumb glow)
- This replaced an earlier 12-bit pitch-class bitmask that lit up all octaves of the same note

### Maqam Selector & Preset System
- Two-dropdown selector: base maqam (searchable) + variant/transposition
- `CustomSelect` component: searchable with keyboard nav (ArrowUp/Down/Enter/Escape), highlighted option auto-scrolls
- Preset labels use non-breaking hyphen (U+2011) after "al" to prevent line breaks: `.replace(/\bal-/gi, 'al\u2011')`
- Preset compatibility: when switching tuning systems, presets are checked against `maqamList` — if `preset.maqamId` doesn't exist or `transpositionIndex` is out of bounds, preset is disabled (opacity 0.35, cursor not-allowed)
- `degreeNames` (ascending PAO names) stored in presets for degree highlighting

### Maqam List & Sets Caching
- `ApiDataCache` caches both maqam list and twelve-pitch-class sets per tuning system
- **Lazy loading**: `loadFromDisk()` only scans filenames into `lazyKeys` set — actual JSON deserialization happens on first `getData()` call via `ensureLoaded()`
- **Incremental saves**: each `storeData()`, `updateSets()`, `updateMaqamList()`, `updateLastChecked()` immediately writes the modified entry to disk (no destructor-only saving)
- Cache directory: `~/Library/ArabicMaqamTuner/cache/` (JUCE `userApplicationDataDirectory` + child dirs)
- `fetchMaqamListIfNeeded()` runs at the **start** of `loadTuningSystem()` (parallel with pitch class fetch, not sequential)
- `fetchSetsIfNeeded()` runs at the **end** of the `doLoad()` lambda (after pitch classes are processed)
- Both check cache first, fall back to API, then update cache on success

## JUCE 8 WebView Bridge

Native functions are registered via `Options::withNativeFunction()` at construction time (not post-construction). The `NativeBridge` must be constructed before `WebBrowserComponent`:

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

JUCE 8 injects a `./juce` ES module at runtime with `getNativeFunction`, `getSliderState`, etc. In our code:
- `ui/src/hooks/juce.d.ts` — TypeScript declarations for the runtime module
- `ui/src/hooks/useJuceBridge.ts` — uses dynamic `import('./juce')` with try/catch for dev mode fallback
- Events: `window.__JUCE__.backend.addEventListener(eventId, callback)` — callback receives data directly (not wrapped in `{ data: string }`)

## API

Base URL: `https://diarmaqar.netlify.app/api`

Key endpoints:
- `GET /tuning-systems` — list all systems
- `GET /tuning-systems/{id}/{startingNote}/pitch-classes?pitchClassDataType=all` — all pitch data
- `GET /maqamat/classification/12-pitch-class-sets?tuningSystem={id}&startingNote={note}&pitchClassDataType=midiNoteDeviation` — classification sets
- `GET /maqamat?tuningSystem={id}&startingNote={note}` — maqam list with transpositions and degrees

### Response structure (important for parsing)

**`/tuning-systems`**: `{ count, data: [{ tuningSystem: { id, idName, displayName, version, year }, startingNotes: { idNames: [...], displayNames: [...] }, stats: {...} }] }`

**`/pitch-classes`**: `{ tuningSystem: {...}, pitchClasses: [...] }` — field is `midiNotePlusCentsDeviation` (not `midiNoteDeviation`), and `ipnReferenceNoteName` is provided by the API

**`/12-pitch-class-sets`**: `{ statistics: {...}, sets: [{ sourceMaqam: { idName, displayName }, pitchClassSet: [...], compatibleMaqamat: [{ tonic: { ipnReferenceNoteName, ... } }] }] }`

MIDI deviation format: `"48 -5.9"` = MIDI note 48, -5.9 cents from 12-EDO.

## MTS-ESP API

- `MTS_FilterNote(bool doFilter, char midinote, signed char midichannel)` — 3 args
- `MTS_ClearNoteFilter()` — 0 args, clears all filters
- `MTS_SetNoteTunings(const double* freqs)` — 128-note frequency table
- **IPC recovery**: If `MTS_CanRegisterMaster()` fails but `MTS_HasIPC()` is true, stale shared memory from a crashed session is likely. Call `MTS_Reinitialize()` then `MTS_RegisterMaster()` to recover. See `MtsEspTransmitter` constructor

**Terminology**: We use **Transmitter** (not Master) and **Receiver** (not Client/Slave) throughout our codebase. The underlying MTS-ESP C API still uses `MTS_RegisterMaster`, `MTS_GetNumClients`, etc. — those are third-party and unchanged.

- C++ wrapper: `MtsEspTransmitter` (in `source/engine/MtsEspTransmitter.h/.cpp`)
- JSON keys: `isMtsTransmitter`, `mtsReceivers`
- TS types: `tuningState.isMtsTransmitter`, `tuningState.mtsReceivers`

## Receiver Plugin

The **Receiver** plugin (`ArabicMaqamTunerReceiver` CMake target) is a lightweight MIDI effect for non-MTS-ESP synths:
- Registers as MTS-ESP Client via `MTS_RegisterClient()` / `MTS_DeregisterClient()`
- Queries tuning per-block: `MTS_RetuningInSemitones()` × 128 notes → cents deviation table
- Filters notes via `MTS_ShouldFilterNote()` (unmapped notes skipped)
- Two modes: MPE (channels 2-16, per-note pitch bend) and Mono Pitch Bend (single channel)
- Separate pitch bend ranges: MPE default 48st, Mono default 2st
- APVTS for DAW automation + state save/recall (3 params: mode, mpePbRange, monoPbRange)
- `IS_MIDI_EFFECT=TRUE`, `IS_SYNTH=FALSE` — appears in MIDI FX slots (Logic, Reaper)
- Simple JUCE-native editor (~360×210px): mode combo, PB range slider, connection status
- Status updates at 5Hz: checks `MTS_HasMaster()` + `MTS_GetScaleName()`
- **Ableton**: Needs a Max for Live wrapper (future work) — Ableton doesn't support VST3 MIDI effects natively

## Conventions

- JUCE `MidiBufferIterator` has no `operator->`. Use `(*it).getMessage()` or range-for with `meta.getMessage()`.
- Test executables: one per test file (each has its own `main()`), `JUCE_STANDALONE_APPLICATION=0`.
- `DiArMaqArClient` needs `<juce_events/juce_events.h>` for `MessageManager::callAsync`.
- **Transmitter plugin classification**: `IS_SYNTH=TRUE`, `IS_MIDI_EFFECT=FALSE`, `isMidiEffect()=false`, `VST3_CATEGORIES "Instrument" "Tools"`. This allows placement on MIDI tracks without requiring another instrument before it (like ODDSound MTS-ESP Master).
- **Receiver plugin classification**: `IS_SYNTH=FALSE`, `IS_MIDI_EFFECT=TRUE`, `isMidiEffect()=true`, `VST3_CATEGORIES "Tools"`. MIDI effect placed before synths in the signal chain.
- **Audio bus config**: constructor uses `BusesProperties().withInput("Input", stereo, true).withOutput("Output", stereo, true)`. Ableton requires at least one audio bus to load any VST3. Both `processBlock` overloads call `audio.clear()` to silence the buffer (as an instrument, the DAW sends uninitialized audio data).
- **Post-build codesign**: CMake builds leave a broken code signature ("sealed resource missing"). Must re-sign before Ableton can load: `codesign --force --deep --sign - "build/ArabicMaqamTuner_artefacts/Debug/VST3/Arabic Maqam Tuner.vst3"`
- **Ableton MIDI routing limitation**: MPE/Pitch Bend data does not pass between tracks (Ableton merges all MIDI to channel 1). MTS-ESP is the recommended output mode for Ableton — it works globally without MIDI routing. MPE/Pitch Bend modes are for DAWs that support placing MIDI effects before instruments (Logic, Reaper, etc.).

## Max for Live Wrapper

The M4L wrapper is needed because Ableton doesn't support VST3 MIDI effects natively. Max's `vst~` cannot output pitch bend from VST3 plugins (`kLegacyMIDICCOutEvent` dropped, input MIDI echoed). Architecture follows ODDSound's proven approach:

### Architecture
1. **Receiver VST3** (`vst~`) — pure MTS-ESP data bridge: reads tuning, exposes 128 cents parameters (`cents_0`–`cents_127`, range ±4800)
2. **Max `js` object** (`m4l/mts_midi_effect.js`) — all MIDI processing: note tracking, MPE channel allocation (ch 2-16 round-robin), 14-bit pitch bend, Mono PB mode
3. **Max patch** (`m4l/Arabic Maqam Tuner Receiver.maxpat`) — `midiin` → `midiparse` → `js` → `midiout`; `midiformat` for CC passthrough; `vst~` parameter polling for tuning data

### Patch Generation
- Generated via **py2max** (`pip3 install py2max`) — `python3 m4l/generate_patch.py`
- py2max's MaxRef skips Ableton-bundled Max (`if "Ableton" not in str(p)`) — all object metadata specified manually
- Post-processing fixes: comments/midiout `numoutlets=0`, remove `order` from patchlines, add `openrect` for M4L

### Parameter Bridge (C++)
- 128 `AudioParameterFloat` params in Receiver APVTS (`cents_0`–`cents_127`)
- Updated from audio thread via `setValueNotifyingHost()` — VST3 spec allows this
- Rate-limited: every 10 processBlock calls, only changed values (>0.01 cents threshold)
- Max polling: `uzi 128 0` → `+ 3` → `prepend get` → `vst~` outlet 3 → `unpack` → denormalize (`$f1 * 9600 - 4800`) → `js` inlet 1

### MaxMSP MCP Server
For interactive Max patch development via Claude: `/Users/khyamallami/code_projects/MaxMSP-MCP-Server`
- Requires Max 9 with demo.maxpat open (Socket.IO server on port 5002)
- 16 tools: `add_max_object`, `connect_max_objects`, `get_objects_in_patch`, `get_object_doc`, etc.
- Configured at user scope for Claude Code CLI

```
m4l/
  generate_patch.py                    py2max script to regenerate .maxpat
  mts_midi_effect.js                   Max js object (MIDI processing)
  Arabic Maqam Tuner Receiver.maxpat   Generated Max patch
  Arabic Maqam Tuner Receiver.amxd     Frozen M4L device (for distribution)
```
