#pragma once
#include "model/ActiveTuningState.h"
#include "model/MaqamPreset.h"
#include "model/TuningSystem.h"
#include "api/DiArMaqArClient.h"
#include "api/ApiDataCache.h"
#include "api/DataUpdateChecker.h"
#include "engine/TuningEngine.h"
#include "receiver/ReceiverRegistry.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// Number of chromatic slot parameters (slot_0 through slot_11)
inline constexpr int kNumSlotParams = 12;
// Number of preset choices ("None" + presets 1-16)
inline constexpr int kNumPresetChoices = 17;

/** Check whether a Processor is still alive (for use in callAsync lambdas). */
inline bool isAlive (const std::weak_ptr<std::atomic<bool>>& w)
{
    auto f = w.lock();
    return f && f->load (std::memory_order_acquire);
}

class ArabicMaqamTunerProcessor : public juce::AudioProcessor,
                                   public juce::AudioProcessorValueTreeState::Listener,
                                   public juce::MidiInputCallback
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

    void setSliderVariant  (int chromaticIndex, int variantIndex);
    void setNoteVariant    (int midiNote, int variantIndex);
    void setSlotCents      (int chromaticIndex, double centsValue);
    void finalizeSlotCents (int chromaticIndex, double centsValue);
    void applyPreset      (int presetIndex);
    void assignPreset     (int presetIndex, const juce::String& maqamIdName,
                           const juce::String& maqamDisplayName,
                           const juce::String& baseMaqamIdName,
                           bool isTransposed,
                           const juce::String& tonicNoteName,
                           const juce::String& tonicIpnRef,
                           const juce::String& tonicSolfege,
                           int pitchClassSetIndex,
                           const std::array<int, 12>& sliderPositions,
                           const std::vector<juce::String>& degreeNames,
                           const std::array<double, 12>& centsOffsets,
                           const juce::String& tuningSystemId,
                           const juce::String& startingNote);
    void clearPreset      (int presetIndex);
    void applyMaqam       (const juce::String& maqamId, int transpositionIndex);

    // ── Maqam/scroll state (synced from JS, persisted in session) ────────────
    void setStartMidi (double startMidi);

    // ── State accessors ───────────────────────────────────────────────────────
    const std::vector<TuningSystem>&         getTuningSystems()        const;
    const std::vector<MaqamListEntry>&       getMaqamList()            const;
    const ActiveTuningState&                 getActiveTuningState()    const;
    const std::array<MaqamPreset, 16>&       getPresets()              const;
    juce::String                             getCurrentSystemId()      const;
    juce::String                             getCurrentStartingNote()  const;
    const std::vector<PitchClass>&           getCurrentPitchClasses()  const;
    bool                                     isMtsTransmitter()        const;
    int                                      mtsNumReceivers()         const;
    ReceiverCounts                           getReceiverCounts()       const;

    juce::String                             getCurrentMaqamId()           const { return currentMaqamId; }
    int                                      getCurrentTranspositionIdx()  const { return currentTranspositionIdx; }
    int                                      getCurrentActivePresetIdx()   const { return currentActivePresetIdx; }
    double                                   getCurrentStartMidi()         const { return currentStartMidi; }
    const std::vector<juce::String>&         getCurrentDegreeNames()       const { return currentDegreeNames; }
    bool                                     getSessionRecallInProgress()  const { return sessionRecallInProgress; }
    bool                                     getHasRecalledSessionState()  const { return hasRecalledSessionState; }
    const std::array<juce::String, 12>&      getDegreeIpnRefs()            const { return currentDegreeIpnRefs; }
    const std::array<juce::String, 12>&      getDegreeSolfegeRefs()        const { return currentDegreeSolfegeRefs; }

    // Maqam display info (for MIDI export)
    juce::String                             getCurrentMaqamDisplay()      const { return currentMaqamDisplay; }
    juce::String                             getCurrentTonicDisplay()      const { return currentTonicDisplay; }
    juce::String                             getCurrentTonicEnglish()      const { return currentTonicEnglish; }
    juce::String                             getCurrentTonicSolfege()      const { return currentTonicSolfege; }

    // ── Data update checker ───────────────────────────────────────────────────
    void checkForDataUpdates (std::function<void (std::vector<juce::String>)> onUpdatesFound,
                              std::function<void()> onNoUpdates,
                              std::function<void (juce::String)> onError = {});

    // ── Change notifications (for NativeBridge → WebView) ────────────────────
    std::function<void()> onTuningStateChanged;
    std::function<void()> onTuningSystemsLoaded;
    std::function<void()> onMaqamListLoaded;
    std::function<void (juce::String)> onStatusMessage;
    // Lightweight callback for slot cents changes (DAW automation at high rate)
    std::function<void (int chromaticIndex, double centsOffset)> onSlotCentsChanged;

    // ── MIDI activity (audio thread → editor via atomic) ───────────────────
    // 128-bit bitmask (4 × 32-bit words) for per-note MIDI activity.
    // noteOnBits accumulates Note Ons, noteOffBits accumulates Note Offs.
    // Editor exchanges each word to 0 every tick.
    std::atomic<uint32_t> noteOnBits[4]  = {};
    std::atomic<uint32_t> noteOffBits[4] = {};

    // ── MIDI note → preset triggering (MIDI Learn) ─────────────────────────────
    // Each preset can be mapped to a specific MIDI note (-1 = unmapped)
    // midiPresetChannel: 0 = any channel, 1-16 = specific channel
    std::array<std::atomic<int>, 16> midiPresetNotes;  // Note per preset, -1 = unmapped
    std::atomic<int> midiPresetChannel { 0 };          // 0 = any channel
    std::atomic<int> pendingMidiPreset { -1 };         // Set by MIDI callback, consumed by editor timer
    std::atomic<int> midiLearnTargetPreset { -1 };     // -1 = not learning, 0-15 = learning for preset

    void setMidiPresetNote (int presetIdx, int midiNote);
    int  getMidiPresetNote (int presetIdx) const;
    void setMidiPresetChannel (int channel);
    int  getMidiPresetChannel() const { return midiPresetChannel.load(); }
    int  consumePendingMidiPreset();  // Returns preset index (0-15) or -1 if none

    // MIDI Learn mode
    void startMidiLearn (int presetIdx);
    void cancelMidiLearn();
    int  getMidiLearnTarget() const { return midiLearnTargetPreset.load(); }
    bool isMidiLearning() const { return midiLearnTargetPreset.load() >= 0; }
    void clearMidiPresetNote (int presetIdx);
    void clearAllMidiPresetNotes();

    // ── Direct MIDI device input (bypasses DAW MIDI routing) ─────────────────
    // Allows channel filtering to work in Ableton (which normalizes to ch1)
    juce::StringArray getAvailableMidiDevices() const;
    juce::String      getMidiPresetDevice() const;
    void              setMidiPresetDevice (const juce::String& deviceName);

    // MidiInputCallback override
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    // ── APVTS for DAW automation / MIDI mapping ───────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Parameter pointers for quick access (non-owning)
    std::array<juce::AudioParameterFloat*, kNumSlotParams> slotParams {};
    juce::AudioParameterChoice* presetParam = nullptr;

    // ── APVTS Listener ───────────────────────────────────────────────────────
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // ── Gesture marking for DAW automation recording ─────────────────────────
    void beginSliderGesture (int chromaticIndex);
    void endSliderGesture   (int chromaticIndex);
    void beginPresetGesture();
    void endPresetGesture();

private:
    // ── Core state ────────────────────────────────────────────────────────────
    juce::String              currentSystemId;
    juce::String              currentStartingNote;
    ActiveTuningState         activeTuningState;
    std::array<MaqamPreset, 16> presets;
    std::vector<MaqamListEntry>      currentMaqamList;
    juce::String              currentMaqamDisplay;   // e.g. "maqām rāst"
    juce::String              currentTonicDisplay;   // e.g. "rāst" (or transposition tonic)
    juce::String              currentTonicEnglish;   // e.g. "C3" (IPN from pitch class data)
    juce::String              currentTonicSolfege;   // e.g. "Do 3" (solfège from pitch class data)

    // ── Maqam selection state (persisted in session + disk) ─────────────────
    juce::String              currentMaqamId;              // "maqam_rast" or "" if none
    int                       currentTranspositionIdx = -1; // -1 = base tonic
    int                       currentActivePresetIdx  = -1; // -1 = none
    double                    currentStartMidi       = 48.0; // scroll position (synced from JS)
    std::vector<juce::String> currentDegreeNames;           // ascending PAO names for highlighting
    bool                      hasRecalledSessionState = false;
    bool                      sessionRecallInProgress = false;
    int                       currentTonicChromatic  = 0;   // 0-11, used for tonic-relative slot mapping

    // ── Maqam detail data (context-aware IPN references + solfege) ─────────
    std::array<juce::String, 12> currentDegreeIpnRefs;              // per chromatic slot, empty = no override
    std::array<juce::String, 12> currentDegreeSolfegeRefs;          // per chromatic slot, empty = no override
    std::map<juce::String, juce::String> currentTranspositionIdMap;  // tonicId → transposition idName

    // ── Direct MIDI device input for preset triggering ──────────────────────
    std::unique_ptr<juce::MidiInput> midiPresetInput;
    juce::String                     midiPresetDeviceName;  // Empty = disabled

    // ── Lifetime guard (must be declared before apiClient so it outlives it) ─
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // ── Subsystems ────────────────────────────────────────────────────────────
    DiArMaqArClient  apiClient;
    ApiDataCache     dataCache;
    DataUpdateChecker updateChecker;
    TuningEngine     tuningEngine;

    // ── Internal helpers ──────────────────────────────────────────────────────
    void rebuildTuningStateFromCache();
    void fetchMaqamListIfNeeded();
    void fetchAndApplyMaqamDetail();
    void applyDegreeIpnRefs (const MaqamDetailResult& detail);
    void applyMaqamDegrees (const MaqamDegrees& degrees);
    void notifyTuningChanged();
    juce::String buildScaleName() const;

    // ── APVTS sync helpers ───────────────────────────────────────────────────
    // Guard flag to prevent feedback loops when programmatically updating params
    bool updatingParamsFromCode = false;

    // Tonic-relative slot mapping: slot_0 = tonic, slot_1 = tonic+1, etc.
    // When no maqam is selected, defaults to C (chromatic 0).
    int slotToChromatic (int slotIdx) const { return (slotIdx + currentTonicChromatic) % 12; }
    int chromaticToSlot (int chromaticIdx) const { return (chromaticIdx - currentTonicChromatic + 12) % 12; }

    // Sync APVTS params from current tuning state
    void syncSlotParamFromState (int chromaticIndex);
    void syncAllSlotParamsFromState();
    void syncPresetParamFromState();

    // ── Disk persistence ─────────────────────────────────────────────────────
    void saveSettingsToDisk() const;
    void loadSettingsFromDisk();
    void savePresetsToDisk() const;
    void loadPresetsFromDisk();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArabicMaqamTunerProcessor)
};
