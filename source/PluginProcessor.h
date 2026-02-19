#pragma once
#include "model/ActiveTuningState.h"
#include "model/MaqamPreset.h"
#include "model/TuningSystem.h"
#include "api/DiArMaqArClient.h"
#include "api/ApiDataCache.h"
#include "api/DataUpdateChecker.h"
#include "engine/TuningEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>

class ArabicMaqamTunerProcessor : public juce::AudioProcessor
{
public:
    ArabicMaqamTunerProcessor();
    ~ArabicMaqamTunerProcessor() override;

    // ── AudioProcessor interface ──────────────────────────────────────────────
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tanghim"; }
    bool   acceptsMidi()   const override { return true; }
    bool   producesMidi()  const override { return true; }
    bool   isMidiEffect()  const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int    getNumPrograms() override { return 1; }
    int    getCurrentProgram() override { return 0; }
    void   setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void   changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── Tuning control (called from NativeBridge on message thread) ───────────
    void loadTuningSystem (const juce::String& systemId,
                           const juce::String& startingNote,
                           std::function<void()> onComplete = {});

    void setSliderVariant (int chromaticIndex, int variantIndex);
    void setNoteVariant   (int midiNote, int variantIndex);
    void applyPreset      (int presetIndex);
    void assignPreset     (int presetIndex, const juce::String& maqamIdName,
                           const juce::String& maqamDisplayName,
                           const juce::String& baseMaqamIdName,
                           bool isTransposed,
                           const juce::String& tonicNoteName,
                           const juce::String& tonicIpnRef,
                           int pitchClassSetIndex,
                           const std::array<int, 12>& sliderPositions,
                           const std::vector<juce::String>& degreeNames);
    void clearPreset      (int presetIndex);
    void applyMaqamFromSet (int setIndex);
    void applyMaqam       (const juce::String& maqamId, int transpositionIndex);

    // ── State accessors ───────────────────────────────────────────────────────
    const std::vector<TuningSystem>&         getTuningSystems()        const;
    const std::vector<TwelvePitchClassSet>&  getTwelvePitchClassSets() const;
    const std::vector<MaqamListEntry>&       getMaqamList()            const;
    const ActiveTuningState&                 getActiveTuningState()    const;
    const std::array<MaqamPreset, 12>&       getPresets()              const;
    juce::String                             getCurrentSystemId()      const;
    juce::String                             getCurrentStartingNote()  const;
    const std::vector<PitchClass>&           getCurrentPitchClasses()  const;
    bool                                     isMtsTransmitter()        const;
    int                                      mtsNumReceivers()         const;

    // ── Data update checker ───────────────────────────────────────────────────
    void checkForDataUpdates (std::function<void (std::vector<juce::String>)> onUpdatesFound,
                              std::function<void()> onNoUpdates,
                              std::function<void (juce::String)> onError = {});

    // ── Change notifications (for NativeBridge → WebView) ────────────────────
    std::function<void()> onTuningStateChanged;
    std::function<void()> onTuningSystemsLoaded;
    std::function<void()> onMaqamSetsLoaded;
    std::function<void()> onMaqamListLoaded;
    std::function<void (juce::String)> onStatusMessage;

    // ── MIDI activity (audio thread → editor via atomic) ───────────────────
    // 128-bit bitmask (4 × 32-bit words) for per-note MIDI activity.
    // noteOnBits accumulates Note Ons, noteOffBits accumulates Note Offs.
    // Editor exchanges each word to 0 every tick.
    std::atomic<uint32_t> noteOnBits[4]  = {};
    std::atomic<uint32_t> noteOffBits[4] = {};

private:
    // ── Core state ────────────────────────────────────────────────────────────
    juce::String              currentSystemId;
    juce::String              currentStartingNote;
    ActiveTuningState         activeTuningState;
    std::array<MaqamPreset, 12> presets;
    std::vector<TwelvePitchClassSet> currentSets;
    std::vector<MaqamListEntry>      currentMaqamList;

    // ── Subsystems ────────────────────────────────────────────────────────────
    DiArMaqArClient  apiClient;
    ApiDataCache     dataCache;
    DataUpdateChecker updateChecker;
    TuningEngine     tuningEngine;

    // ── Internal helpers ──────────────────────────────────────────────────────
    void rebuildTuningStateFromCache();
    void fetchSetsIfNeeded();
    void fetchMaqamListIfNeeded();
    void applyMaqamDegrees (const MaqamDegrees& degrees);
    void notifyTuningChanged();
    juce::String buildScaleName() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArabicMaqamTunerProcessor)
};
