#pragma once
#include "PluginProcessor.h"
#include "engine/MidiFileGenerator.h"
#include <juce_gui_extra/juce_gui_extra.h>

/**
 * Builds the WebBrowserComponent Options with all C++ ↔ JS bridge functions
 * pre-registered, and provides helpers to push state to the WebView.
 *
 * C++ → JS (push state):
 *   browser.emitEventIfBrowserIsVisible("tuningStateChanged", buildTuningStateJson())
 *   browser.emitEventIfBrowserIsVisible("tuningSystemsLoaded", buildTuningSystemsJson())
 *   browser.emitEventIfBrowserIsVisible("statusMessage", "Loading…")
 *
 * JS → C++ (native functions, called as Promises):
 *   Juce.getNativeFunction("getTuningSystems")()
 *   Juce.getNativeFunction("selectTuningSystem")(systemId, startingNote)
 *   Juce.getNativeFunction("setSliderVariant")(chromaticIndex, variantIndex)
 *   Juce.getNativeFunction("setNoteVariant")(midiNote, variantIndex)
 *   Juce.getNativeFunction("applyPreset")(presetIndex)
 *   Juce.getNativeFunction("assignPreset")(presetIndex, maqamId, maqamDisplay, sliderPositions[12])
 *   Juce.getNativeFunction("clearPreset")(presetIndex)
 *   Juce.getNativeFunction("checkForUpdates")()
 */
class NativeBridge
{
public:
    NativeBridge (ArabicMaqamTunerProcessor& processor);
    ~NativeBridge() = default;

    /**
     * Returns Options with all native functions registered.
     * Pass this to WebBrowserComponent::Options::withOptionsFrom() or merge manually.
     * Call BEFORE constructing the WebBrowserComponent.
     */
    juce::WebBrowserComponent::Options applyTo (juce::WebBrowserComponent::Options opts) const;

    /** Build the full tuning state as a JSON var for emitting to JS. */
    juce::var buildTuningStateJson() const;

    /** Build the tuning systems list as a JSON var. */
    juce::var buildTuningSystemsJson() const;

    /** Build the maqam list (with degrees + transpositions) as a JSON var. */
    juce::var buildMaqamListJson() const;

private:
    ArabicMaqamTunerProcessor& processor;
    mutable std::unique_ptr<juce::FileChooser> fileChooser;  // Must stay alive during async dialog
    mutable juce::File lastTanghimDirectory;                  // Remembers last save/load directory

    // ── JSON builders ─────────────────────────────────────────────────────────
    juce::var buildPresetsJson()    const;
    juce::var pitchClassToVar (const PitchClass& pc) const;

    /** Build MIDI file data for current maqam (returns null if no maqam selected). */
    juce::var buildMaqamMidiDragData() const;

    /** Save MIDI file to Downloads folder, returns { path, filename } or null. */
    juce::var saveMaqamMidiFile() const;

};
