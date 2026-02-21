# Development Diary

## Feb 22 2026: MIDI Learn for Presets + Modification Tracking Fix

### Features Implemented

#### MIDI Learn for Preset Triggering

Users can now map MIDI notes to presets for instant maqam switching during performance.

**Why a dedicated MIDI input?** VST3 plugins only receive MIDI through DAW routing, which requires a MIDI track to be armed, the plugin in the signal path, and the DAW passing MIDI. For reliable preset triggering during performance, we open a **direct MIDI input device** that bypasses DAW routing entirely. Users select their MIDI controller from a dropdown in the native status bar.

**User interaction:**
- **Select MIDI device** from dropdown in native status bar (required first!)
- **Shift+click** a preset button → enters MIDI Learn mode (button pulses, shows "...")
- Play any MIDI note → that note is mapped to the preset
- **Click the MIDI badge** → clears the mapping
- **Shift+click the badge** → re-learn with a different note

**C++ implementation (`PluginProcessor`):**
- Inherits from `juce::MidiInputCallback`
- `midiPresetInput` (unique_ptr<MidiInput>): direct MIDI device handle
- `handleIncomingMidiMessage()`: processes Note On from direct input
- `midiLearnTargetPreset` (atomic int): which preset is learning (-1 = none)
- `midiPresetNotes[16]` (atomic ints): MIDI note mapped to each preset (-1 = unmapped)
- `midiPresetChannel` (atomic int): channel filter (0 = any, 1-16 = specific)
- `pendingMidiPreset` (atomic int): preset index triggered by MIDI, consumed by editor timer
- `processBlock()` checks Note On messages against mappings, sets `pendingMidiPreset`
- Mappings persisted to `~/Library/Tanghim/settings.json`

**Editor timer (`PluginEditor`):**
- 30Hz poll checks `consumePendingMidiPreset()`
- On trigger: calls `applyPreset()` + `emitTuningStateChanged()`

**Bridge functions (`NativeBridge`):**
- `startMidiLearn(presetIndex)` → returns current learning target
- `cancelMidiLearn()` → cancels learning mode
- `getMidiLearnTarget()` → returns which preset is learning
- `getMidiPresetNote(presetIndex)` → returns mapped MIDI note
- `clearMidiPresetNote(presetIndex)` → clears mapping
- `setMidiPresetChannel(channel)` / `getMidiPresetChannel()`

**React UI (`MaqamPresetButton`):**
- `isMidiLearning` prop: shows pulsing animation + "..." badge
- `midiNote` prop: displays note name (e.g., "C3") when mapped
- Badge click handlers for clear/re-learn

#### Maqam Modification Tracking Fix

**The bug:** Selecting a maqam then modifying a slider should turn the thumb cyan and add `*` to the maqam name. This was broken.

**Root cause (multi-layered):**

1. **C++ timing issue**: In `applyMaqam()`, `currentDegreeNames` was populated AFTER calling `applyMaqamDegrees()`, but `applyMaqamDegrees()` calls `notifyTuningChanged()`. The event was sent with empty degree names.

   **Fix:** Move degree names population BEFORE `applyMaqamDegrees()`:
   ```cpp
   // Store degree names BEFORE applyMaqamDegrees, since it calls notifyTuningChanged()
   currentDegreeNames.clear();
   for (const auto& name : found->degrees.ascending)
       currentDegreeNames.push_back(name);
   applyMaqamDegrees(found->degrees);
   ```

2. **Async APVTS callbacks**: `syncAllSlotParamsFromState()` updates APVTS parameters with `updatingParamsFromCode = true`. But if the DAW calls `parameterChanged` asynchronously after the flag resets to `false`, it would clear maqam state and emit events with empty data.

   **Fix:** Removed maqam state clearing from `parameterChanged` for slot params. The explicit methods (`setSliderVariant`, `setSlotCents`) already handle this for user interactions.

3. **JS state overwrite**: `syncMaqamStateFromCpp` would receive stale async events with empty maqam data AFTER `handleMaqamSelect` had already set the correct state, overwriting it.

   **Fix:** Added guards to preserve existing maqam state:
   ```typescript
   // Don't let empty events clear state we just set
   if (!state.selectedMaqamId && (isMaqamModifiedRef.current || selectedMaqamIdRef.current)) {
     setActivePresetIndex(state.activePresetIndex ?? -1)
     return  // Preserve current maqam state
   }
   ```

### DAW Automation Parameters (APVTS)

The Transmitter plugin exposes 13 parameters for DAW automation and MIDI CC mapping:

| Parameter ID | Type | Range | Purpose |
|---|---|---|---|
| `slot_0`–`slot_11` | AudioParameterFloat | ±100 cents | Cents deviation for each chromatic slot |
| `preset` | AudioParameterChoice | "None", "1"–"16" | Active preset index |

**Key implementation details:**
- **Bidirectional sync**: UI changes update APVTS (for recording); DAW automation updates tuning (for playback)
- **Gesture marking**: `beginSliderGesture()`/`endSliderGesture()` called from JS on mousedown/mouseup for proper automation recording
- **Feedback loop prevention**: `updatingParamsFromCode` flag prevents recursive updates when syncing state
- **Per-note overrides**: UI-only, not exposed as parameters (would be 128 params!)
- **MIDI Learn mappings**: Separate from APVTS — persisted to disk, not DAW session

**Why only 13 params?** Originally considered 16 preset trigger params, but MIDI Learn via dedicated input is more flexible and doesn't pollute automation lanes.

### Key Learnings

1. **APVTS callbacks can be async**: `setValueNotifyingHost()` may trigger `parameterChanged` on a deferred message thread callback, after `updatingParamsFromCode` is already reset. Guard flags need careful lifetime management.

2. **Event ordering matters**: When C++ emits events during an async bridge call, JS receives them DURING the `await`. If the JS code sets state AFTER the await, stale events can overwrite it.

3. **Use refs for callbacks**: React's `useCallback` captures state at creation time. Use refs (`useRef`) updated during render to ensure callbacks always access current values.

4. **Debug with console.log**: Adding logging to `syncMaqamStateFromCpp` revealed the exact sequence of events that caused the bug.

### Files Changed
- `source/PluginProcessor.h/.cpp` (MIDI Learn state, degree names fix, parameterChanged fix)
- `source/PluginEditor.h/.cpp` (MIDI preset controls, timer handling)
- `source/NativeBridge.cpp` (MIDI Learn bridge functions)
- `ui/src/App.tsx` (refs for modification tracking, syncMaqamStateFromCpp guards)
- `ui/src/components/MaqamPresetButton.tsx/.css` (MIDI badge UI)
- `ui/src/components/MaqamPresetBar.tsx` (MIDI Learn props)
- `ui/src/hooks/useJuceBridge.ts` (MIDI Learn bridge calls)
- `ui/src/types/index.ts` (TuningState.midiLearnTarget)

---

## Feb 21 2026: Native MIDI Drag Button + Native Status Bar

### Problem
Needed to add a drag-and-drop MIDI file export feature. When a maqam is selected, users should be able to drag a button to their DAW timeline or desktop to export the maqam scale as a MIDI file.

### Initial Approaches (Failed)

1. **Data URI with base64 encoding**: Created MIDI data in C++, sent to JS as base64, tried to trigger download via data URI. Failed because JUCE's `toBase64Encoding()` includes line breaks, and even after fixing that, WebView's atob() had issues.

2. **Blob URL download**: Converted base64 to Blob, created object URL, triggered download. Failed with "Frame load interrupted" — WebViews don't support blob URL downloads.

3. **Native button overlay on WebView**: Added a native JUCE button with `setAlwaysOnTop(true)` and `toFront()`. Button was invisible because WKWebView on macOS exists in a separate native window hierarchy that JUCE cannot control.

### Solution: Native Status Bar

The only reliable way to have native JUCE components work alongside WKWebView is to **not overlap them**.

**Implementation:**
- Reserved 26px at bottom of editor for native status bar
- WebView bounds: `getLocalBounds().withTrimmedBottom(26)`
- Native `paint()` renders: background, border, version+timestamp, status message
- Native components: `MidiDragButton`, `updatesButton` (TextButton)
- React StatusBar hidden via CSS `display: none`

### MIDI File Generation

Created `MidiFileGenerator` class (`source/engine/MidiFileGenerator.h/.cpp`):
- SMF Type 0 format (single track)
- 96 ticks per quarter note
- All scale degrees as simultaneous chord, held 1 beat
- UTF-8 track name with full maqam/tonic metadata
- VLQ encoding for delta times

**Filename format:** `maqām_rāst_al-rāst_C3_Do3.mid`
- Maqam display name (with diacritics)
- "al-" prefix + tonic PAO name
- Tonic IPN (e.g., C3)
- Tonic solfège (e.g., Do3)

### Drag Implementation

```cpp
void mouseDown (const juce::MouseEvent&) override
{
    // Delete previous temp file
    if (tempMidiFile.existsAsFile())
        tempMidiFile.deleteFile();
    prepareMidiFile();
}

void mouseDrag (const juce::MouseEvent& e) override
{
    if (tempMidiFile.existsAsFile() && e.getDistanceFromDragStart() > 4)
    {
        // Don't delete on completion - DAW needs time to read
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            { tempMidiFile.getFullPathName() }, false, this, nullptr);
    }
}
```

**Critical insight:** The completion callback cannot delete the temp file immediately — DAWs like Ableton don't read the file synchronously during the drag operation. The file must persist until the next drag.

### Key Learnings

1. **WKWebView z-order**: Native macOS WebView exists outside JUCE's component hierarchy. Cannot overlay with JUCE components.

2. **JUCE tempDirectory on macOS**: Maps to `~/Library/Caches/{appName}/`, not `/var/folders/.../T/`

3. **Drag completion timing**: DAWs may read dragged files asynchronously after the drag operation "completes" from the source app's perspective.

4. **BUILD_TIMESTAMP macro**: Generated in `build/generated/BuildTimestamp.h`, must include this header to use.

### Files Changed
- `source/engine/MidiFileGenerator.h/.cpp` (new)
- `source/PluginEditor.h/.cpp` (MidiDragButton, native status bar)
- `source/PluginProcessor.h/.cpp` (accessor methods)
- `source/NativeBridge.cpp` (bridge functions, tonicSolfege in presets)
- `ui/src/components/StatusBar.css` (hidden)
- `CMakeLists.txt` (added MidiFileGenerator.cpp)
