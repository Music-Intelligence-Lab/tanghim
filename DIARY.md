# Development Diary

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
