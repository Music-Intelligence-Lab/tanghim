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

Note: The build timestamp in the status bar is auto-generated on every build via `cmake/GenerateTimestamp.cmake`.

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
    MonoPitchBendProcessor.h/.cpp  14-bit mono pitch bend (shared with Receiver)
    MidiFileGenerator.h/.cpp SMF Type 0 generator for maqam scale export
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
  Tanghim Receiver.maxpat      Generated Max patch
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

### Context-Aware IPN Labels & Solfege (Maqam Detail API)
Slider IPN labels and solfege are context-aware when a maqam is selected — both come from the same maqam detail data source:
- **Problem**: The same pitch at chromatic index 6 should show "F#" in Hijaz (degree III, since G is degree IV) but "Gb" in Saba (degree IV, since F is degree III). IPN and solfege must be consistent (same data source).
- **Solution**: The maqam detail API (`GET /maqamat/{id}?pitchClassDataType=all`) provides per-degree `ipnReferenceNoteName` and `solfege`
- `MaqamDetailResult` struct: `ascendingDegrees` (vector of PitchClass) + `transpositionIdMap` (tonicId → transposition idName)
- `currentDegreeIpnRefs[12]` + `currentDegreeSolfegeRefs[12]`: per chromatic slot overrides from maqam detail
- `degreeIpnMap` + `degreeSolfegeMap` in tuning state JSON: chromatic index → IPN / solfege string
- NoteSliderBank uses degree maps when available, falls back to tuning system data
- Cached per `systemId:startingNote:maqamId` in `~/Library/Tanghim/cache/maqam-detail/`
- For transpositions: base maqam detail provides `availableTranspositions` mapping, then transposition-specific detail is fetched with `&transpositionId=` parameter

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
- Sliders have a **fixed width** of 68px (`ui/src/constants.ts: SLOT_WIDTH_PX`)
- The number of visible sliders is computed dynamically via `ResizeObserver` in `useVisibleSliderCount` hook
- Widening the plugin window reveals more sliders; narrowing hides them — no CSS scrolling, virtual render only
- **Continuous tuning**: Slider thumbs are freely draggable within ±150 cents range. `centsOffset` (per chromatic slot) is the **source of truth** for frequency/cents table computation. `selectedIndex` is retained for snap marker highlighting and PAO name display
- **Maqam modification tracking**: When a maqam is selected and sliders are adjusted, degree highlights remain active (don't clear), maqam name shows ` *` suffix, and modified slider thumbs turn cyan (#26c6da). Reset on maqam/preset/system change
- **Snap markers**: Clickable dots to the left of each slider track — snap to exact tuning system variant values with 0.1s CSS transition. Cursor: `pointer`
- **Thumb cursor**: `ns-resize` (double-arrow vertical) — indicates free drag
- **Live MTS-ESP update**: Tuning updates on every mousemove via fire-and-forget `setSlotCents` bridge call (RAF-throttled ~60fps). `setSlotCentsFinalize` on mouseup returns full TuningState for React state sync
- **Persistence**: `centsOffset` saved per slot in DAW session state. Backward-compatible: old sessions derive `centsOffset` from selected variant's `midiCentsDeviation`
- **Smooth scrolling**: `startMidi` is fractional (not integer), mouse wheel delta proportional to `deltaY / SLOT_WIDTH_PX`. NoteSliderBank renders an extra slider and uses CSS `translateX(BANK_LEFT_OFFSET_PX - pixelOffset)` for sub-pixel offset
- **Left offset alignment**: `BANK_LEFT_OFFSET_PX = 16` in constants.ts — slider bank content is offset 16px from left edge to align with upper sections (which have 16px padding). Available width for sliders = container width - 16px
- `RangeScroller` pans smoothly with `step="any"`, tick marks at every C, G, A. Double-click centers viewport on the maqam's octave
- **Curtain effect centering**: Viewport auto-centers on the maqam's octave (or C3 if no maqam) using `centerMaqamOctave(tonicMidi, visibleCount)`. Formula: `padding = Math.max(0, Math.floor((visibleCount - 13) / 2))`. An octave = 13 sliders (tonic through its octave above). The `Math.max(0, ...)` prevents negative padding when viewport < 13 sliders. Centering is consistent across: plugin load, tuning system select, maqam select, preset load, and range slider double-click
- Plugin window: 832–2400px wide, 620–4000px tall (`PluginEditor.cpp: setResizeLimits`)
- Min width = 16px left offset + 12 sliders × 68px = 832px (one octave aligned with upper sections)
- Min height = maqam dropdown (max-height 480px) lines up flush with status bar
- Default window: 832×620, default start MIDI: 48 (C3), default 12 visible sliders

### Reference Frequency Control
- Global concert pitch offset: `referenceCentsOffset` (±700 cents, default 0)
- Applied as frequency multiplier in `TuningEngine::updateTuning()` after building the 128-note table: `f *= 2^(cents/1200)`
- `referenceNoteMidi` = MIDI note of tonic in octave 1, used for Hz display: `defaultHz = 440 * 2^((midi-69)/12)`
- **Resets to 0 on starting note/system change** — each starting note is a fresh reference point
- UI: SVG arc knob (drag ±cents, Shift=fine, dbl-click=reset), editable Hz input, cents display, ±100 cent semitone buttons
- Bridge: `setReferenceFreqCents` (fire-and-forget), `setReferenceFreqCentsFinalize` (returns state), gesture start/end
- APVTS: `ref_freq` param for DAW automation, saved/restored in session state

### Per-Note Overrides
- Per-MIDI-note variant overrides allow different variants for the same pitch class in different octaves
- Override indicator: blue thumb glow + accent-coloured IPN label
- Stored in `ActiveTuningState::perNoteVariantOverrides[128]`

### MIDI Activity Feedback
- C++ tracks per-note activity via 128-bit atomic bitmask (`uint32_t[4]` for on/off)
- Editor timer (30fps) exchanges bitmasks and sends arrays of MIDI note numbers to JS: `{ on: [60, 64], off: [48] }`
- JS maintains a `Set<number>` of active MIDI notes — highlights only the exact note being played (gold thumb glow)
- This replaced an earlier 12-bit pitch-class bitmask that lit up all octaves of the same note

### Native Status Bar & MIDI Drag Export
The status bar is rendered natively in JUCE (not WebView) to support drag-and-drop functionality:

**Why native?** WKWebView on macOS exists in a separate window hierarchy from JUCE components. Native JUCE components cannot reliably appear on top of the WebView regardless of `setAlwaysOnTop()` or `toFront()` calls. The only solution is to not overlap them.

**Layout:**
- WebView bounds: `getLocalBounds().withTrimmedBottom(26)` — leaves 26px for native status bar
- Native status bar: rendered in `paint()` with version+timestamp (left); right-aligned controls: Preset MIDI Map Config label + device/channel dropdowns, MIDI drag button, Clear Cache button, Updates button
- React StatusBar: hidden via CSS (`display: none`)

**MidiDragButton (PluginEditor.h):**
- Native JUCE component, visible only when a maqam is selected
- `mouseDown`: generates temp MIDI file via `MidiFileGenerator`
- `mouseDrag`: calls `DragAndDropContainer::performExternalDragDropOfFiles()`
- Temp file persists until next `mouseDown` (DAWs need time to read the file)
- File location: `~/Library/Caches/Tanghim/` (JUCE `tempDirectory` on macOS)

**MIDI File Format (MidiFileGenerator):**
- SMF Type 0, single track, 96 ticks/quarter note
- Scale degrees played as chord, held for 1 quarter note
- UTF-8 track name with maqam and tonic info
- Filename: `maqām_rāst_al-rāst_C3_Do3.mid` (maqam display + "al-" + tonic PAO + IPN + solfège)

### Maqam Selector & Preset System
- Two-dropdown selector: base maqam (searchable) + variant/transposition
- `CustomSelect` component: searchable with keyboard nav (ArrowUp/Down/Enter/Escape), highlighted option auto-scrolls
- Transposition dropdown labels: `"segāh / E-b3 / Mi -b3 (qarār)"` — PAO display name + IPN (englishName) + solfège, with "(qarār)" suffix for the base tonic
- Transposition sort order: ascending by tuning system pitch order using compound key `(octave, pitchClassIndex)` from pitch class data
- `paoOrder`: unique PAO idNames sorted by `(octave, pitchClassIndex)` — used for transposition dropdown ordering
- `paoNameInfo`: PAO idName → `{ englishName, solfege }` mapping — used for transposition dropdown labels
- Preset labels use non-breaking hyphen (U+2011) after "al" to prevent line breaks: `.replace(/\bal-/gi, 'al\u2011')`
- Preset compatibility: when switching tuning systems, presets are checked against `maqamList` — if `preset.maqamId` doesn't exist or `transpositionIndex` is out of bounds, preset is disabled (opacity 0.35, cursor not-allowed)
- `degreeNames` (ascending PAO names) stored in presets for degree highlighting
- `centsOffsets` (12 doubles) stored in presets — restores exact slider tuning values even when modified from maqam defaults
- Modified presets include ` *` suffix in display name to indicate custom tuning
- **Tuning system persistence**: Modified presets store `tuningSystemId` and `startingNote`. When loaded in a different system, the plugin switches to the correct system first. Unmodified presets are portable (load in any system using that system's interpretation of the maqam)
- **Async system switch pattern**: When loading a modified preset requiring a system switch, the callback is stored in `afterSystemSwitchRef` and executed when `onTuningStateChanged` fires after the system loads

### Maqam List Caching
- `ApiDataCache` caches maqam list per tuning system
- **Lazy loading**: `loadFromDisk()` only scans filenames into `lazyKeys` set — actual JSON deserialization happens on first `getData()` call via `ensureLoaded()`
- **Incremental saves**: each `storeData()`, `updateSets()`, `updateMaqamList()`, `updateLastChecked()` immediately writes the modified entry to disk (no destructor-only saving)
- Cache directory: `~/Library/Tanghim/cache/` (JUCE `userApplicationDataDirectory` + child dirs)
- `fetchMaqamListIfNeeded()` runs **after** pitch classes are loaded in `doLoad()` (pitch classes are the critical path for slider display)
- Checks cache first, falls back to API, then updates cache on success

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
- `GET /tuning-systems/{id}/{startingNote}/maqamat?includeMaqamDegrees=true&includeTranspositions=true` — maqam list with degrees
- `GET /maqamat/{maqamId}?tuningSystem={id}&startingNote={note}&pitchClassDataType=all` — maqam detail with context-aware IPN
- `GET /maqamat/{maqamId}?tuningSystem={id}&startingNote={note}&pitchClassDataType=all&transpositionId={idName}` — transposed maqam detail

### Response structure (important for parsing)

**`/tuning-systems`**: `{ count, data: [{ tuningSystem: { id, idName, displayName, version, year }, startingNotes: { idNames: [...], displayNames: [...] }, stats: {...} }] }`

**`/pitch-classes`**: `{ tuningSystem: {...}, pitchClasses: [...] }` — field is `midiNotePlusCentsDeviation` (not `midiNoteDeviation`), `ipnReferenceNoteName` and `solfege` are provided by the API

MIDI deviation format: `"48 -5.9"` = MIDI note 48, -5.9 cents from 12-EDO.

## MTS-ESP API

- `MTS_FilterNote(bool doFilter, char midinote, signed char midichannel)` — 3 args
- `MTS_ClearNoteFilter()` — 0 args, clears all filters
- `MTS_SetNoteTunings(const double* freqs)` — 128-note frequency table
- **IPC recovery**: If `MTS_CanRegisterMaster()` fails but `MTS_HasIPC()` is true, stale shared memory from a crashed session is likely. Call `MTS_Reinitialize()` then `MTS_RegisterMaster()` to recover. See `MtsEspTransmitter` constructor

**Terminology**: We use **Transmitter** (not Master) and **Receiver** (not Client/Slave) throughout our codebase. The underlying MTS-ESP C API still uses `MTS_RegisterMaster`, `MTS_GetNumClients`, etc. — those are third-party and unchanged.

- C++ wrapper: `MtsEspTransmitter` (in `source/engine/MtsEspTransmitter.h/.cpp`)
- JSON keys: `isMtsTransmitter`, `mtsReceivers`, `mtsNativeCount`, `mpeCount`, `monoPbCount`
- TS types: `tuningState.isMtsTransmitter`, `tuningState.mtsNativeCount`, `tuningState.mpeCount`, `tuningState.monoPbCount`
- Lightweight event: `mtsStatusChanged` (polled at 2Hz, separate from full `tuningStateChanged`)

## Receiver Plugin

The **Receiver** plugin (`ArabicMaqamTunerReceiver` CMake target) is a lightweight MIDI effect for non-MTS-ESP synths:
- Registers as MTS-ESP Client via `MTS_RegisterClient()` / `MTS_DeregisterClient()`
- Queries tuning per-block: `MTS_RetuningInSemitones()` × 128 notes → cents deviation table
- Filters notes via `MTS_ShouldFilterNote()` (unmapped notes skipped)
- Two modes: MPE (channels 2-16, per-note pitch bend) and Mono Pitch Bend (single channel)
- Separate pitch bend ranges: MPE default 48st, Mono default 2st
- APVTS for DAW automation + state save/recall (3 params: mode, mpePbRange, monoPbRange)
- `IS_MIDI_EFFECT=TRUE`, `IS_SYNTH=FALSE` — appears in MIDI FX slots (Logic, Reaper)
- JUCE-native editor (~360×210px) with dark navy theme matching Transmitter: toggle buttons (MPE/Mono PB), PB range with +/- buttons and editable value, connection status
- Status updates at 5Hz: checks `MTS_HasMaster()` + `MTS_GetScaleName()`
- **Ableton**: Needs a Max for Live wrapper (future work) — Ableton doesn't support VST3 MIDI effects natively
- **Registry**: Writes `{uuid}.mpe` or `{uuid}.monopb` to `~/Library/Tanghim/receivers/` for Transmitter discovery (file-based IPC since plugins are separate shared libraries)

### Receiver Type Discovery (File-Based Registry)

MTS-ESP only provides `MTS_GetNumClients()` — a single integer count with no client enumeration. To distinguish MTS-ESP-native synths from our Receivers (and their modes), we use a file-based registry:

- **Directory**: `~/Library/Tanghim/receivers/`
- **File naming**: `{uuid}.mpe` or `{uuid}.monopb` — extension encodes mode
- **Receiver side**: announce on construct, 1Hz heartbeat (mtime touch), switchMode on APVTS change, deannounce on destruct
- **Transmitter side**: 2Hz scan in editor timer, stale files (>5s mtime) ignored, periodic cleanup (>10s) every ~30 seconds
- **Count computation**: MTS-ESP native = `MTS_GetNumClients()` - (MPE count + Mono PB count)
- **UI**: 3 conditional badge chips with color coding (MTS-ESP=accent, MPE=blue, Mono PB=green)
- **Crash recovery**: Heartbeat stops → file becomes stale → ignored by scanner → deleted by cleanup

### MTS-ESP Status Polling

The Transmitter's 30Hz editor timer includes a 2Hz MTS-ESP status poll that emits a lightweight `mtsStatusChanged` event to the WebView when any count changes. This is separate from the full `tuningStateChanged` event to avoid pushing the entire tuning state (12 slots, 128 overrides, etc.) on every status check.

## Plugin Naming

**DAW-facing names must be pure ASCII.** Ableton's VST3 scanner cannot handle UTF-8 in plugin names — non-ASCII characters (ī, en-dash, etc.) display as garbled CJK characters. Additionally, JUCE's CMake post-build scripts break with parentheses `()` in PRODUCT_NAME (shell syntax error in `cmake -E remove`).

- **PRODUCT_NAME**: `"Tanghim"` / `"Tanghim Receiver"` (ASCII only)
- **WebView UI title**: `"Tanghīm"` (UTF-8 fine — rendered by WebKit, not the DAW)
- **CMake targets**: `ArabicMaqamTuner` / `ArabicMaqamTunerReceiver` (internal, unchanged)
- **Extended ASCII** (ISO 8859-1 / Windows-1252) does NOT contain ī (U+012B, Latin i with macron). Macron vowels only exist in ISO 8859-4 (Baltic), which DAWs don't use.

## Conventions

- **Lifetime guard pattern**: All `MessageManager::callAsync` lambdas capturing `this` (Processor) must capture `std::weak_ptr<std::atomic<bool>> weak(alive)` and check `if (!isAlive(weak)) return;` at the top. This prevents use-after-free when Processor is destroyed while async callbacks are pending. The `alive` flag is declared BEFORE `apiClient` so it outlives the background thread.
- **Background cache preloading**: When warm cache has data on disk but not in memory (`hasData` but `!isInMemory`), use `apiClient.runOnThread()` to deserialize JSON on the background thread, then `callAsync` back to message thread. This avoids blocking the message thread during JSON parsing.
- JUCE `MidiBufferIterator` has no `operator->`. Use `(*it).getMessage()` or range-for with `meta.getMessage()`.
- Test executables: one per test file (each has its own `main()`), `JUCE_STANDALONE_APPLICATION=0`.
- `DiArMaqArClient` needs `<juce_events/juce_events.h>` for `MessageManager::callAsync`.
- **Transmitter plugin classification**: `IS_SYNTH=TRUE`, `IS_MIDI_EFFECT=FALSE`, `isMidiEffect()=false`, `VST3_CATEGORIES "Instrument" "Tools"`. This allows placement on MIDI tracks without requiring another instrument before it (like ODDSound MTS-ESP Master).
- **Receiver plugin classification**: `IS_SYNTH=FALSE`, `IS_MIDI_EFFECT=TRUE`, `isMidiEffect()=true`, `VST3_CATEGORIES "Tools"`. MIDI effect placed before synths in the signal chain.
- **Audio bus config**: constructor uses `BusesProperties().withInput("Input", stereo, true).withOutput("Output", stereo, true)`. Ableton requires at least one audio bus to load any VST3. Both `processBlock` overloads call `audio.clear()` to silence the buffer (as an instrument, the DAW sends uninitialized audio data).
- **Post-build codesign**: CMake builds leave a broken code signature ("sealed resource missing"). Must `rm -rf` the old VST3 in `~/Library/Audio/Plug-Ins/VST3/` before `cp -R` (overwriting in-place corrupts kernel signature cache → SIGKILL under Rosetta), then `codesign --force --deep --sign -` the installed copy.
- **Ableton MIDI routing limitation**: MPE/Pitch Bend data does not pass between tracks (Ableton merges all MIDI to channel 1). MTS-ESP is the recommended output mode for Ableton — it works globally without MIDI routing. MPE/Pitch Bend modes are for DAWs that support placing MIDI effects before instruments (Logic, Reaper, etc.).
- **JUCE DynamicObject ownership**: Never pass `unique_ptr<DynamicObject>::get()` to `juce::var` or `juce::JSON::toString()` — `juce::var` takes ref-counted ownership and will delete the object, causing a double-free when `unique_ptr` also deletes it. Use `new DynamicObject()` and pass directly to `juce::var(obj)`, or use `unique_ptr::release()`.

### Ableton Live Debugging

- **User version**: Live 11.3.43
- **Plugin scanner log**: `~/Library/Preferences/Ableton/Live 11.3.43/PluginScanner.txt`
- **System crash reports**: `/Library/Logs/DiagnosticReports/Ableton Plugin Scanner-*.ips` (macOS-level, plugin scanner process)
- **Ableton crash reports**: `~/Library/Application Support/Ableton/Live Reports/` (zipped crash reports with timestamps — check here first for Live crashes)
- **Plugin database**: `~/Library/Application Support/Ableton/Live Database/Live-plugins-1.db.bak`
- Scanner runs x86_64 under Rosetta on Apple Silicon

### Known Issue: WKWebView + Ableton Computer MIDI Keyboard

**Status: UNRESOLVED** — documented for future investigation.

When the Tanghim plugin window has been interacted with (specifically after using a text input like the searchable maqam dropdown), Ableton's Computer MIDI Keyboard experiences timing delays. External MIDI controllers and arpeggiators are unaffected regardless of plugin focus state.

**Root cause**: WKWebView's internal `WKContentView` becomes first responder when any text input is focused. Once it has first responder status, it retains it even after the text input loses focus, interfering with keyboard event delivery to the DAW host.

**Reproduction**: Load plugin → play CMK with plugin focused → works fine. Use searchable maqam dropdown → close dropdown → play CMK → timing delay. Click away from plugin window to unfocus → CMK works fine again.

**Approaches attempted (all unsuccessful)**:
1. `browser->setWantsKeyboardFocus(false)` — JUCE component-level, doesn't affect native WKContentView
2. `EDITOR_WANTS_KEYBOARD_FOCUS FALSE` in CMakeLists.txt — VST3 flag, doesn't prevent WKContentView FR
3. ObjC swizzle of `WKWebView` keyDown/keyUp → wrong target (WKContentView handles keys)
4. ObjC swizzle of `WKContentView` keyDown/keyUp → keys still delayed even when forwarded to nextResponder
5. Swizzle `WKContentView::acceptsFirstResponder` to return NO + `resignWebViewFirstResponder()` on search close → still triggers after text input interaction, cause unclear
6. Release build (embedded BinaryData vs Vite dev server) → same issue, rules out dev server overhead

**Workaround for users**: Click anywhere outside the plugin window to remove focus, or use an external MIDI controller. The issue only affects Ableton's Computer MIDI Keyboard when the plugin window is focused AND has had text input interaction.

**References**:
- JUCE forum: https://forum.juce.com/t/fixing-webview-keyboard-focus-issue/63987
- JUCE forum: https://forum.juce.com/t/webview-and-keyboard-input-propagation-issue-to-the-host/62439
- JUCE GitHub: https://github.com/juce-framework/JUCE/issues/1522

### Release Build (BinaryData Resource Provider)

The resource provider for serving embedded UI in release builds requires careful handling:
- JUCE's BinaryData **removes** dashes from filenames (not replaces with `_`): `index-BUhML9SX.css` → `indexBUhML9SX_css`
- Must match against `BinaryData::originalFilenames[]` instead of replicating JUCE's name mangling
- The callback receives a path like `/assets/index-Bzx_Xwfh.js` — strip directory prefix before BinaryData lookup
- `withResourceProvider` second argument (`allowedOriginIn`) is optional — omit it for resource provider mode
- See the JUCE `WebViewPluginDemo.h` example for reference implementation

## Max for Live Wrapper

The M4L wrapper is needed because Ableton doesn't support VST3 MIDI effects natively. Max's `vst~` cannot output pitch bend from VST3 plugins (`kLegacyMIDICCOutEvent` dropped, input MIDI echoed). Architecture follows ODDSound's proven approach:

### Architecture
1. **Receiver VST3** (`vst~`) — pure MTS-ESP data bridge: reads tuning, exposes 128 cents parameters (`cents_0`–`cents_127`, range ±4800)
2. **Max `js` object** (`m4l/mts_midi_effect.js`) — all MIDI processing: note tracking, MPE channel allocation (ch 2-16 round-robin), 14-bit pitch bend, Mono PB mode
3. **Max patch** (`m4l/Tanghim Receiver.maxpat`) — `midiin` → `midiparse` → `js` → `midiout`; `midiformat` for CC passthrough; `vst~` parameter polling for tuning data

### Patch Generation
- Generated via **py2max** (`pip3 install py2max`) — `python3 m4l/generate_patch.py`
- py2max's MaxRef skips Ableton-bundled Max (`if "Ableton" not in str(p)`) — all object metadata specified manually
- Post-processing fixes: comments/midiout `numoutlets=0`, remove `order` from patchlines, add `openrect` for M4L

### Parameter Bridge (C++)
- 128 `AudioParameterFloat` params in Receiver APVTS (`cents_0`–`cents_127`)
- Updated from audio thread via `setValueNotifyingHost()` — VST3 spec allows this
- Rate-limited: every 10 processBlock calls, only changed values (>0.01 cents threshold)
- **JUCE VST3 bypass parameter**: JUCE adds an automatic bypass param at index 0, so APVTS params start at index 1. With 3 control params (mode, mpePbRange, monoPbRange), `cents_0` is at **VST3 index 4** (not 3)
- Max polling: `uzi 128 0` → `+ 4` → `prepend get` → `vst~` outlet 3 → `unpack` → `- 4` (MIDI note) + denormalize (`$f1 * 9600 - 4800` = cents) → `pack` → `js` inlet 1

### Ableton MPE Flag
- **`is_mpe: 1`** must be set on the M4L patcher metadata — without it, Ableton normalizes all MIDI output to channel 1, breaking MPE
- ODDSound's two M4L devices (MPE/non-MPE) are structurally identical; only `is_mpe` and default voice mode differ
- Safe to always set `is_mpe: 1` even when using Mono PB mode (channel 1 only)

### Note Off Pitch Bend
- **Never reset pitch bend on Note Off** — causes audible snap during synth release tail
- Next Note On always sets PB before sounding, so no stale value issue

### M4L Install & Deploy
- **Install folder**: `~/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect/Tanghim/`
- Files needed: `Tanghim Receiver.amxd` + `mts_midi_effect.js`
- **Must delete old files before copying new** (same as VST3 install pattern)
- Regenerate: `python3 m4l/generate_patch.py` then copy `.amxd` + `.js` to install folder

### MaxMSP MCP Server
For interactive Max patch development via Claude: `/Users/khyamallami/code_projects/MaxMSP-MCP-Server`
- Requires Max 9 with demo.maxpat open (Socket.IO server on port 5002)
- 16 tools: `add_max_object`, `connect_max_objects`, `get_objects_in_patch`, `get_object_doc`, etc.
- Configured at user scope for Claude Code CLI

```
m4l/
  generate_patch.py                    py2max script to regenerate .maxpat
  mts_midi_effect.js                   Max js object (MIDI processing)
  Tanghim Receiver.maxpat               Generated Max patch
  Tanghim Receiver.amxd                 Frozen M4L device (for distribution)
```

## APVTS: MIDI-Mappable & Automatable Sliders + Presets

The Transmitter plugin has 14 APVTS parameters exposed for DAW automation and MIDI CC mapping:

| Parameter ID | Type | Range | Default | Purpose |
|---|---|---|---|---|
| `slot_0`–`slot_11` | `AudioParameterFloat` | -100.0 to +100.0 cents | 0.0 | Cents deviation for each chromatic slot |
| `ref_freq` | `AudioParameterFloat` | -700.0 to +700.0 cents | 0.0 | Global reference frequency offset (concert pitch) |
| `preset` | `AudioParameterChoice` | "None", "1"–"16" (17 choices) | 0 (None) | Active preset index |

**Key behaviors:**
- Bidirectional sync: UI changes update APVTS params (for DAW recording); DAW automation updates tuning state (for playback)
- Gesture marking: `beginSliderGesture()`/`endSliderGesture()` and `beginRefFreqGesture()`/`endRefFreqGesture()` called from JS on mousedown/mouseup for proper DAW automation recording
- Feedback loop prevention: Single guard flag (`updatingParamsFromCode`) prevents recursive updates
- Per-note overrides remain UI-only (not exposed as parameters)

## MIDI Learn: Per-Preset Note Mapping

MIDI Learn allows mapping MIDI notes to presets for instant maqam switching during performance. This is separate from APVTS automation — mappings are persisted to disk, not to DAW session state.

### Dedicated MIDI Input Device

**Why a separate input?** VST3 plugins only receive MIDI through the DAW's routing, which requires:
- A MIDI track to be selected/armed
- The plugin to be in the MIDI signal path
- The DAW to be in a state that passes MIDI (not all do when stopped)

For reliable preset triggering during performance, we open a **direct MIDI input** that bypasses DAW routing entirely.

**Native status bar controls (label: "Preset MIDI Map Config:"):**
- **MIDI Input dropdown**: Select from available MIDI devices (or "None" to disable)
- **Channel dropdown**: Filter by channel (1-16) or "All" for any channel
- **Clear Cache button**: Clears all cached API data (`dataCache.clearAll()`) — both `~/Library/Tanghim/cache/` and `~/Library/Tanghim/cache/maqam-detail/`

**Device management (`PluginProcessor`):**
- `midiPresetInput` (unique_ptr<MidiInput>): Direct MIDI input device
- `midiPresetDeviceName` (String): Selected device name, persisted to settings
- Processor inherits from `juce::MidiInputCallback`
- `handleIncomingMidiMessage()`: Processes Note On from direct input
- Device/channel saved to `~/Library/Tanghim/settings.json`

### User Interaction

- **Shift+click preset button** → enters MIDI Learn mode (button pulses, badge shows "...")
- **Play any MIDI note** → maps that note to the preset, exits learn mode
- **Click MIDI badge** → clears the mapping
- **Shift+click badge** → re-learn with a different note

### C++ State (`PluginProcessor`)
- `midiLearnTargetPreset` (atomic int): preset currently learning (-1 = none)
- `midiPresetNotes[16]` (atomic int array): MIDI note mapped to each preset (-1 = unmapped)
- `midiPresetChannel` (atomic int): channel filter (0 = any channel, 1-16 = specific)
- `pendingMidiPreset` (atomic int): preset triggered by MIDI, consumed by editor timer

**Processing flow:**
1. `processBlock()` checks Note On messages against `midiPresetNotes[]` mappings
2. On match: sets `pendingMidiPreset` (atomic, lock-free)
3. Editor timer (30Hz) calls `consumePendingMidiPreset()`
4. On consume: calls `applyPreset()` + `emitTuningStateChanged()`

**Persistence:**
- Mappings saved to `~/Library/Tanghim/settings.json` as `midiPresetNotes` array
- Channel saved as `midiPresetChannel`
- Loaded on plugin startup via `loadSettingsFromDisk()`

**Bridge functions:**
- `startMidiLearn(presetIndex)` → begins learning for preset
- `cancelMidiLearn()` → cancels learning mode
- `getMidiLearnTarget()` → returns which preset is learning (-1 if none)
- `getMidiPresetNote(presetIndex)` → returns mapped MIDI note (-1 if unmapped)
- `clearMidiPresetNote(presetIndex)` → clears mapping for preset
- `clearAllMidiPresetNotes()` → clears all mappings
- `setMidiPresetChannel(channel)` / `getMidiPresetChannel()` → channel filter

**Why not APVTS?**
- MIDI note mappings are user preferences, not musical content to automate
- Mappings should persist across DAW sessions without needing to save the project
- Avoids polluting automation lanes with 16+ non-musical parameters
