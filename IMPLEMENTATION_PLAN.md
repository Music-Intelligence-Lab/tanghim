# Arabic Maqam Tuner — VST Plugin Implementation Plan

## Context

Build a cross-platform VST3/AU/CLAP microtuning plugin using JUCE (C++) that fetches tuning data from the DiArMaqAr API and applies maqam-based microtuning via MTS-ESP, MPE, and 14-bit monophonic pitch bend. The plugin maps Arabic maqam pitch classes to a 12-key MIDI keyboard using the 12-pitch-class-set classification system.

**Why this matters:** No dedicated, open-source VST exists for Arabic maqam microtuning with proper musicological logic. This bridges the DiArMaqAr research platform with practical music production.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    Plugin Editor (GUI)                    │
│  ┌──────────────┐ ┌──────────┐ ┌──────────────────────┐ │
│  │ TuningSystem │ │ Output   │ │ Starting Note        │ │
│  │ Selector     │ │ Mode     │ │ Selector             │ │
│  └──────────────┘ └──────────┘ └──────────────────────┘ │
│  ┌──────────────────────────────────────────────────────┐│
│  │  [P1] [P2] [P3] [P4] [P5] [P6] [P7] [P8] ... [P12]││
│  │  12 Maqam Preset Buttons                            ││
│  └──────────────────────────────────────────────────────┘│
│  ┌──────────────────────────────────────────────────────┐│
│  │  C    C#   D    D#   E    F    F#   G    G#   A   A#  B ││
│  │  [|]  [|]  [|]  [|]  [|]  [|]  [|]  [|]  [|]  [|] [|] [|]││
│  │  12 Vertical Sliders (snap to pitch class variants) ││
│  └──────────────────────────────────────────────────────┘│
│  ┌──────────────────────────────────────────────────────┐│
│  │  Status bar                                          ││
│  └──────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────┘
         │                    │
         ▼                    ▼
┌─────────────────┐  ┌─────────────────────┐
│ DiArMaqAr API   │  │ Tuning Engine       │
│ Client + Cache  │  │ ├─ MTS-ESP Master   │
│                 │  │ ├─ MPE Processor    │
│ (bg thread)     │  │ └─ Mono PB Proc    │
└─────────────────┘  └─────────────────────┘
                            │
                     ┌──────┴──────┐
                     │ processBlock │
                     │ (MIDI I/O)  │
                     └─────────────┘
```

---

## Critical Musicological Logic

**IPN Reference Note Assignment** (from DiArMaqAr source code):
- Microtonal modifiers indicate what a pitch is a **variant OF**, not proximity to 12-EDO
- `E-b` (E half-flat) → variant of **E**, not Eb
- `B-b` (B half-flat) → variant of **B**, not Bb
- Chromatic order uses sharps internally: `C, C#, D, D#, E, F, F#, G, G#, A, A#, B`
- The API's `englishName` field is the primary source of truth for IPN reference

**12-Pitch-Class Set Logic** (from `classifyMaqamat12PitchClassSets.ts`):
1. Extract chromatic pitch classes from tuning system (filter out microtonal ones)
2. Build a 12-note base set (from tuning system's chromatic subset, or al-Kindi fallback)
3. Replace matching IPN positions with maqam's actual pitch classes
4. Reorder chromatically starting from maqam tonic
5. Compatible maqamat share the same 12-note set (within 5 cents tolerance)

**Slider Variants**: Each of the 12 chromatic positions maps to all pitch classes in the tuning system whose `englishName` matches that IPN reference (chromatic, not microtonal). For Ibn Sina:
- E has 2 variants: `E` (būselīk/ʿushshāq) and `E-b` (segāh) — both are variants of E
- D# has 2 variants: `D#` (zīrgūleh) and `D-#` (nīm zīrgūleh)
- C has 1 variant (locked slider)

---

## UI Architecture: JUCE 8 WebView

The plugin UI is built using **JUCE 8's WebView integration** (`juce_gui_extra`). Instead of native JUCE components, the GUI is a web app (React + TypeScript) rendered in the platform's native WebView (WebKit/macOS, Edge/Windows, GTK WebKit2/Linux). C++ and JavaScript communicate via a bidirectional native bridge.

**Why WebView:**
- Rich, custom UI with CSS animations and Arabic text rendering (diacritics, RTL) trivially handled
- Hot reload during development — edit CSS/React without recompiling C++
- Easier to achieve the visual quality needed for the 12-slider layout
- JUCE 8's parameter attachment system (`WebSliderRelay`, etc.) handles knob/slider sync automatically

**Bridge pattern:**
- C++ exposes functions to JS via `.withNativeFunction("name", callback)`
- JS calls them as Promises: `Juce.getNativeFunction("name")(args)`
- C++ pushes state to JS via `emitEventIfBrowserIsVisible("event", data)`
- Dev: `goToURL("http://localhost:5173")` — hot reload from Vite dev server
- Release: resource provider serves bundled `dist/` from `BinaryData`

## Project Structure

```
arabic_maqam_tuner/
├── CMakeLists.txt
├── libs/
│   ├── JUCE/                          (git submodule)
│   ├── clap-juce-extensions/          (git submodule)
│   └── MTS-ESP/                       (git submodule)
├── source/
│   ├── PluginProcessor.h/.cpp         Main processor
│   ├── PluginEditor.h/.cpp            Main editor (hosts WebBrowserComponent)
│   ├── NativeBridge.h/.cpp            All C++↔JS function registrations
│   ├── model/
│   │   ├── PitchClass.h               Single pitch with MIDI+cents
│   │   ├── TuningSystem.h             Tuning system metadata
│   │   ├── TwelvePitchClassSet.h      12-note set with compatible maqamat
│   │   ├── ActiveTuningState.h        Current slider positions → tuning table
│   │   └── MaqamPreset.h              Stored preset (maqam + slider positions)
│   ├── api/
│   │   ├── DiArMaqArClient.h/.cpp     Async HTTP client (bg thread)
│   │   ├── ApiDataCache.h/.cpp        Disk-backed cache
│   │   ├── ApiResponseParser.h/.cpp   JSON → model structs
│   │   └── DataUpdateChecker.h/.cpp   Version-based update detection
│   └── engine/
│       ├── TuningEngine.h/.cpp        Builds 128-note tables, routes output
│       ├── MtsEspMaster.h/.cpp        MTS-ESP master wrapper
│       ├── MpePitchBendProcessor.h/.cpp  MPE channel allocation + bend
│       └── MonoPitchBendProcessor.h/.cpp 14-bit mono pitch bend
├── ui/                                Web frontend (React + TypeScript + Vite)
│   ├── package.json
│   ├── vite.config.ts
│   ├── src/
│   │   ├── main.tsx
│   │   ├── App.tsx
│   │   ├── components/
│   │   │   ├── TuningSystemSelector.tsx
│   │   │   ├── NoteSlider.tsx         Vertical snap slider
│   │   │   ├── NoteSliderBank.tsx     12 sliders
│   │   │   ├── MaqamPresetButton.tsx
│   │   │   ├── MaqamPresetBar.tsx
│   │   │   ├── MaqamPickerDialog.tsx
│   │   │   ├── OutputModeSelector.tsx
│   │   │   └── StatusBar.tsx
│   │   ├── hooks/
│   │   │   └── useJuceBridge.ts       JUCE native function wrappers
│   │   └── types/
│   │       └── index.ts               Shared TS types matching C++ model
│   └── dist/                          Built output (bundled into BinaryData)
└── tests/
    ├── ApiResponseParserTest.cpp
    ├── TuningEngineTest.cpp
    └── MidiProcessingTest.cpp
```

---

## Implementation Milestones

### Milestone 0: Prerequisites
**Goal**: Set up tooling before writing any code

1. **Context7 MCP**: Add Context7 MCP server for up-to-date JUCE documentation:
   ```
   claude mcp add context7 -- npx -y @upstash/context7-mcp@latest
   ```
   This gives us live JUCE API docs in-context during implementation.

2. **Verify Node.js 18+** is available (required by Context7 MCP)

### Milestone 1: Project Skeleton
**Goal**: Plugin loads in DAW with a working WebView window, passes audio+MIDI through

1. `git init`, `.gitignore`, add JUCE + clap-juce-extensions + MTS-ESP as submodules
2. Create `CMakeLists.txt`:
   - `IS_MIDI_EFFECT FALSE`, `IS_SYNTH FALSE` (for DAW compat — Ableton, FL Studio don't support MIDI effects)
   - `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT TRUE`
   - `NEEDS_WEBVIEW2 TRUE` (Windows)
   - Targets: VST3, AU (macOS), Standalone, CLAP
   - Link: `juce_audio_utils`, `juce_audio_processors`, `juce_gui_extra`, `juce_core`
   - Include MTS-ESP Master sources directly
   - CMake custom target to build `ui/` via `npm run build` and embed `dist/` as `BinaryData`
3. Scaffold `ui/` as a Vite + React + TypeScript project (`npm create vite@latest ui -- --template react-ts`)
4. Stub `PluginProcessor` — passthrough audio buffer unchanged, forward MIDI
5. Stub `PluginEditor` — hosts `WebBrowserComponent` pointed at:
   - Debug: `http://localhost:5173` (Vite dev server, hot reload)
   - Release: resource provider serving bundled `dist/` from `BinaryData`
6. Stub `ui/src/App.tsx` — static "Hello Arabic Maqam Tuner" page confirming WebView works
7. Verify: plugin loads in DAW, WebView renders the stub page, MIDI passes through

**CMake WebView setup:**
```cmake
juce_add_plugin(ArabicMaqamTuner
    ...
    NEEDS_WEBVIEW2 TRUE
)
target_compile_definitions(ArabicMaqamTuner PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
)
```

**PluginEditor WebView init:**
```cpp
WebBrowserComponent::Options options;
options = options
    .withBackend(WebBrowserComponent::Options::Backend::webview2)  // Windows
    .withNativeIntegrationEnabled()
    .withResourceProvider(...)  // serves dist/ in release
    .withInitialisationData("version", ProjectInfo::versionString);
browser = std::make_unique<WebBrowserComponent>(options);
#if DEBUG
browser->goToURL("http://localhost:5173");
#else
browser->goToURL(WebBrowserComponent::getResourceProviderRoot());
#endif
```

### Milestone 2: Data Model + API Layer
**Goal**: Fetch and cache tuning system data from DiArMaqAr API

**Data Model** (`source/model/`):

`PitchClass.h`:
```cpp
struct PitchClass {
    int pitchClassIndex;
    int octave;
    juce::String noteName;           // "rast", "segah"
    juce::String noteNameDisplay;    // "rāst", "segāh"
    juce::String englishName;        // "C3", "E-b3", "D#3"
    double cents;                    // Relative cents from tonic
    double frequency;                // Hz
    int midiNoteNumber;              // From midiNoteDeviation: "48" part
    double midiCentsDeviation;       // From midiNoteDeviation: "-5.9" part
    juce::String ipnReference;       // Computed: "C", "E", "D#" etc.
    juce::String fraction;           // "32/27"
};
```

`TuningSystem.h`:
```cpp
struct TuningSystem {
    juce::String id;                 // "ibnsina_1037"
    juce::String displayName;        // "Ibn Sīnā (1037)..."
    int pitchClassesPerOctave;       // 17
    juce::StringArray startingNoteIds;
    juce::StringArray startingNoteDisplayNames;
    double referenceFrequency;       // Hz
};
```

`TwelvePitchClassSet.h`:
```cpp
struct CompatibleMaqam {
    juce::String maqamIdName;
    juce::String maqamDisplayName;
    bool isTransposed;
    juce::String tonicNoteName;
};

struct TwelvePitchClassSet {
    juce::String sourceMaqamIdName;
    juce::String sourceMaqamDisplayName;
    std::array<PitchClass, 12> slots; // Indexed by chromatic position 0-11
    std::vector<CompatibleMaqam> compatibleMaqamat;
};
```

`ActiveTuningState.h`:
```cpp
struct ChromaticNoteVariants {
    juce::String ipnReference;       // "C", "C#", ... "B"
    struct Variant {
        PitchClass pitchClass;
    };
    std::vector<Variant> variants;   // All variants for this IPN ref
    int selectedIndex = 0;           // Current slider position
};

struct ActiveTuningState {
    std::array<ChromaticNoteVariants, 12> slots;
    std::array<double, 128> buildFrequencyTable() const;
    std::array<double, 128> buildCentsDeviationTable() const;
};
```

**API Integration** (`source/api/`):

Key API calls (from OpenAPI spec):
1. `GET /tuning-systems` → list all systems with starting notes
2. `GET /tuning-systems/{id}/{startingNote}/pitch-classes?pitchClassDataType=all` → all pitch data
3. `GET /maqamat/classification/12-pitch-class-sets?tuningSystem={id}&startingNote={note}&pitchClassDataType=midiNoteDeviation` → classification sets

`DiArMaqArClient`: juce::Thread with request queue, callbacks via MessageManager::callAsync()
`ApiDataCache`: In-memory + disk (JSON files in app data dir), keyed by `tuningSystemId:startingNote`
`ApiResponseParser`: Parse `midiNoteDeviation` string format `"48 -5.9"` → `{48, -5.9}`

**Data Update Checker** (`source/api/DataUpdateChecker.h/.cpp`):
The API includes ISO 8601 `version` timestamps on every resource (e.g. `"2025-10-18T19:41:17.132Z"`). The update checker:

1. On plugin startup (or manual "Check for Updates" button), fetch `GET /tuning-systems` and compare each system's `version` field against the stored version in cache
2. If any `version` timestamp is newer than cached, mark that system as stale
3. For stale systems, re-fetch pitch classes and 12-pitch-class-sets data in background
4. Also fetch `GET /maqamat` and compare maqam versions — if any maqam version changed, re-fetch affected classification sets
5. Store the version timestamps alongside cached data in `ApiDataCache`
6. Show update status in the GUI status bar ("Data up to date" / "Updating..." / "X systems updated")

```cpp
class DataUpdateChecker {
public:
    // Check for updates against cached versions
    void checkForUpdates(
        const ApiDataCache& cache,
        DiArMaqArClient& client,
        std::function<void(std::vector<juce::String> updatedSystemIds)> onUpdatesFound,
        std::function<void()> onNoUpdates
    );

    // Force refresh all data
    void forceRefreshAll(DiArMaqArClient& client, ApiDataCache& cache);

    bool isChecking() const;
};
```

The cache stores version timestamps per entry:
```cpp
struct CachedTuningData {
    juce::String tuningSystemVersion;    // ISO 8601 from API
    juce::String lastChecked;            // When we last checked for updates
    // ... pitch class data, sets, etc.
};
```

**Building slider variants from API data**:
1. Fetch all pitch classes with `pitchClassDataType=all` (gives `englishName`, `cents`, `midiNoteDeviation`, `frequency`)
2. For each pitch class, compute IPN reference from `englishName` using same logic as DiArMaqAr:
   - Chromatic names (A2, Bb3, C#4) → extract note + accidental
   - Microtonal names (E-b3, D-#3) are NOT chromatic — they are variants
3. Group all pitch classes by their IPN reference → these become slider variants
4. Each chromatic IPN position (C, C#, D, D#, E, F, F#, G, G#, A, A#, B) gets its grouped pitch classes as slider options

### Milestone 3: Tuning Engine
**Goal**: Convert slider state to frequency tables, output via MTS-ESP/MPE/PB

`TuningEngine.h/.cpp`:
- Maintains 128-note frequency table and cents deviation table
- When slider changes → recalculate: `freq[midiNote] = 440 * 2^((midiNote - 69) * 100 + centsDeviation) / 1200)`
- `centsDeviation` for a MIDI note = the deviation from the selected variant for `midiNote % 12`
- Routes to active output: MTS-ESP, MPE, or mono pitch bend

`MtsEspMaster.h/.cpp`:
- Wraps `libMTSMaster.h` C API
- `MTS_RegisterMaster()` / `MTS_DeregisterMaster()`
- `MTS_SetNoteTunings(frequencies)` — pushes 128-note table
- `MTS_SetScaleName()` — display name for clients
- `MTS_CanRegisterMaster()` — check if another master exists
- `MTS_GetNumClients()` — show in status bar

`MpePitchBendProcessor.h/.cpp`:
- MPE lower zone: channels 2-16 for member notes, channel 1 for manager
- On Note On (ch1): allocate MPE channel, send pitch bend, re-emit note on allocated channel
- On Note Off: re-emit on same channel, release
- Pitch bend range: 48 semitones (covers largest Arabic microtonal deviations)
- 14-bit bend value: `8192 + (int)(centsDeviation / (48 * 100.0) * 8192.0)`
- Round-robin channel allocation with voice stealing

`MonoPitchBendProcessor.h/.cpp`:
- Insert 14-bit pitch bend before each Note On on same channel
- Configurable pitch bend range (user must match synth setting)
- Monophonic only — last note wins

### Milestone 4: WebView GUI
**Goal**: Full interactive React UI connected to C++ backend via JUCE native bridge

**C++ side — `NativeBridge.h/.cpp`** registers all bridge functions on the `WebBrowserComponent`:

```cpp
// C++ → JS: push state whenever tuning changes
browser.emitEventIfBrowserIsVisible("tuningStateChanged", buildTuningStateJson());
browser.emitEventIfBrowserIsVisible("tuningSystemsLoaded", buildSystemsJson());
browser.emitEventIfBrowserIsVisible("updateStatus", "Checking...");

// JS → C++: user actions
.withNativeFunction("getTuningSystems",    [...])  // returns cached list
.withNativeFunction("selectTuningSystem",  [...])  // triggers API load
.withNativeFunction("selectStartingNote",  [...])
.withNativeFunction("setSliderVariant",    [...])  // chromaticIndex, variantIndex
.withNativeFunction("applyPreset",         [...])  // presetIndex
.withNativeFunction("assignPreset",        [...])  // presetIndex, maqamIdName
.withNativeFunction("setOutputMode",       [...])  // "mts-esp" | "mpe" | "pitch-bend"
.withNativeFunction("checkForUpdates",     [...])
```

**JS side — `ui/src/hooks/useJuceBridge.ts`:**
```typescript
const loadPreset = Juce.getNativeFunction("applyPreset");
await loadPreset(slotIndex);

// Subscribe to C++ events
window.__JUCE__.backend.addEventListener("tuningStateChanged", (e) => {
  setTuningState(JSON.parse(e.data));
});
```

**Layout (800x500, React components):**
- **Top bar** (`App.tsx`): `<TuningSystemSelector>` | `<StartingNoteSelector>` | `<OutputModeSelector>`
- **Preset bar** (`MaqamPresetBar.tsx`): 12 `<MaqamPresetButton>` — left-click applies, right-click opens `<MaqamPickerDialog>`
- **Slider bank** (`NoteSliderBank.tsx`): 12 `<NoteSlider>` vertical sliders (C through B)
  - Each shows IPN reference at top, Arabic note name (with diacritics) at each snap position
  - Single-variant sliders rendered locked/disabled
  - Snap positions calculated from variant count: evenly distributed vertically
- **Status bar** (`StatusBar.tsx`): tuning system name | active maqam | MTS-ESP client count | `<UpdateButton>`

**`<MaqamPickerDialog>`**: Modal listing compatible maqamat grouped by 12-pitch-class-set, with search. Selecting a maqam calls `assignPreset(presetIndex, maqamIdName)` → C++ resolves slider positions from the set and returns them.

**Development workflow:**
1. Run Vite dev server: `cd ui && npm run dev`
2. C++ debug build points WebView to `http://localhost:5173`
3. Edit React/CSS → instant hot reload, no C++ recompile needed
4. When ready: `npm run build` → CMake embeds `dist/` as `BinaryData`

### Milestone 5: State & Polish
**Goal**: DAW recall works, edge cases handled, cross-platform

- `getStateInformation` / `setStateInformation` via ValueTree XML
- Persist: tuning system ID, starting note, output mode, 12 slider positions, 12 preset assignments
- On restore: load tuning system (from cache or API), then apply slider positions
- Handle: API unavailable (use cache), MTS-ESP master conflict (fallback to MPE), no variants (lock slider)
- `DataUpdateChecker`: auto-check on startup, manual "Check for Updates" button, re-fetch stale systems using version timestamps
- Cross-platform build verification (macOS, Windows, Linux)

---

## Key API Endpoints (from OpenAPI spec)

| Endpoint | Purpose | Key Params |
|----------|---------|------------|
| `GET /tuning-systems` | List all systems | `includeArabic` |
| `GET /tuning-systems/{id}/{startingNote}/pitch-classes` | All pitch data | `pitchClassDataType=all`, `octave` |
| `GET /maqamat` | List maqamat | `filterByFamily`, `filterByTonic` |
| `GET /maqamat/{idName}` | Maqam detail | `tuningSystem`, `startingNote`, `pitchClassDataType` |
| `GET /maqamat/{idName}/transpositions` | Available transpositions | `tuningSystem`, `startingNote` |

Response format for `midiNoteDeviation`: string `"48 -5.9"` = MIDI note 48, -5.9 cents from 12-EDO.

Valid `pitchClassDataType` values: `all`, `cents`, `fraction`, `midiNoteDeviation`, `midiNoteNumber`, `frequency`, `referenceNoteName`, `englishName`, `centsDeviation`, `decimalRatio`, `stringLength`, `fretDivision`, `abjadName`, `solfege`

---

## Key Files from DiArMaqAr Source (Reference)

These files contain the authoritative logic we must replicate in C++:
- `classifyMaqamat12PitchClassSets.ts` — 12-pitch-class-set classification algorithm
- `calculateIpnReferenceMidiNote.ts` — IPN reference calculation (musicological logic)
- `getIpnReferenceNoteName.ts` — IPN reference extraction
- `PitchClass.ts` — Data model with all fields including `midiNoteDeviation`, `englishName`

---

## Verification Plan

1. **Build**: `cmake -B build && cmake --build build` → produces VST3, AU, Standalone, CLAP
2. **Load in DAW**: Plugin appears in DAW, accepts MIDI input, passes audio through
3. **API fetch**: Select tuning system → data loads, sliders populate with correct variants
4. **Slider behavior**: E slider in Ibn Sina system has 2 positions (segāh, būselīk)
5. **MTS-ESP**: Load MTS-ESP client synth, verify tuning changes when sliders move
6. **MPE**: Send MIDI → verify per-note pitch bend on separate channels
7. **Mono PB**: Send MIDI → verify pitch bend before each Note On
8. **Preset**: Right-click preset → pick maqam → sliders snap → left-click restores
9. **State recall**: Save DAW project, reopen → all settings restored
10. **Cross-platform**: Build and test on macOS + Windows
