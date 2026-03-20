#pragma once
#include "model/ActiveTuningState.h"
#include "model/MaqamPreset.h"
#include "model/TuningSystem.h"
#include "api/DiArMaqArClient.h"
#include "api/ApiDataCache.h"
#include "api/DataUpdateChecker.h"
#include "engine/TuningEngine.h"
#include "engine/TriangleOscillator.h"
#include "receiver/ReceiverRegistry.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

// Number of chromatic slot parameters (slot_0 through slot_11)
inline constexpr int kNumSlotParams = 12;
// APVTS preset param: "None" + presets 1..kNumMaqamPresets
inline constexpr int kNumPresetChoices = 1 + kNumMaqamPresets;

/** Check whether a Processor is still alive (for use in callAsync lambdas). */
inline bool isAlive (const std::weak_ptr<std::atomic<bool>>& w)
{
    auto f = w.lock();
    return f && f->load (std::memory_order_acquire);
}

class TanghimProcessor : public juce::AudioProcessor,
                                   public juce::AudioProcessorValueTreeState::Listener,
                                   public juce::MidiInputCallback
{
public:
    TanghimProcessor();
    ~TanghimProcessor() override;

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
    double getTailLengthSeconds() const override { return 0.15; }
    int    getNumPrograms() override { return 1; }
    int    getCurrentProgram() override { return 0; }
    void   setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void   changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── Internal reference oscillator ──────────────────────────────────────────
    std::atomic<bool> oscillatorEnabled { false };
    void setOscillatorEnabled (bool enabled);
    bool getOscillatorEnabled() const { return oscillatorEnabled.load (std::memory_order_relaxed); }

    // ── Heptatonic keyboard mode ────────────────────────────────────────────────
    std::atomic<bool> heptEnabled { false };
    void setHeptEnabled (bool enabled);
    bool getHeptEnabled() const { return heptEnabled.load (std::memory_order_relaxed); }
    void rebuildHeptMap();

    // ── Reference frequency control ───────────────────────────────────────────
    void   setReferenceCentsOffset (double cents);
    void   finalizeReferenceCentsOffset (double cents);
    double getReferenceCentsOffset() const { return referenceCentsOffset; }
    double getReferenceDefaultHz() const;
    double getReferenceCurrentHz() const;
    juce::String getReferenceNoteDisplayName() const { return referenceNoteDisplayName; }
    int    getReferenceNoteMidi() const { return referenceNoteMidi; }
    void   beginRefFreqGesture();
    void   endRefFreqGesture();

    // ── Tuning control (called from NativeBridge on message thread) ───────────
    void loadTuningSystem (const juce::String& systemId,
                           const juce::String& startingNote,
                           std::function<void()> onComplete = {});

    void setSliderVariant  (int chromaticIndex, int variantIndex);
    void setNoteVariant    (int midiNote, int variantIndex);
    void setSlotCents      (int chromaticIndex, double centsValue);
    void finalizeSlotCents (int chromaticIndex, double centsValue);
    void setNoteCents      (int midiNote, double centsValue);
    void finalizeNoteCents (int midiNote, double centsValue);
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
    bool                                     hasCachedTuningData (const juce::String& systemId, const juce::String& startingNote) const;
    /** True after loadTuningSystem has populated slots (safe to skip reload when reopening the editor). */
    bool                                     hasTuningLoadedForCurrentSelection() const;
    const std::vector<MaqamListEntry>&       getMaqamList()            const;
    const ActiveTuningState&                 getActiveTuningState()    const;
    const std::array<MaqamPreset, kNumMaqamPresets>& getPresets()      const;
    juce::String                             getCurrentSystemId()      const;
    juce::String                             getCurrentStartingNote()  const;
    const std::vector<PitchClass>&           getCurrentPitchClasses()  const;
    bool                                     isMtsTransmitter()        const;
    int                                      mtsNumReceivers()         const;
    ReceiverCounts                           getReceiverCounts()       const;

    juce::String                             getCurrentMaqamId()           const { return currentMaqamId; }
    int                                      getCurrentTranspositionIdx()  const { return currentTranspositionIdx; }
    int                                      getCurrentActivePresetIdx()   const { return currentActivePresetIdx; }
    void                                     setCurrentActivePresetIdx (int idx)  { currentActivePresetIdx = idx; }
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
    /** @return false if a check was already in progress (callbacks not invoked). */
    bool checkForDataUpdatesOnly (
        std::function<void (std::vector<juce::String>)> onStaleFound,
        std::function<void()>                            onAllCurrent,
        std::function<void (juce::String)>               onError = {});

    /** True after any check-only staleness pass completes (auto or manual). */
    bool hasAutoDataUpdateCheckCompleted() const noexcept
    {
        return autoDataUpdateCheckDone.load (std::memory_order_relaxed);
    }

    // ── Download & connection state (editor polls via timer) ─────────────
    enum class DownloadState { idle, downloading, connectionError };
    enum class UpdateState { idle, checking, updatesAvailable, updating, updated, upToDate, error };

    std::atomic<DownloadState> downloadState { DownloadState::idle };
    std::atomic<UpdateState>   updateState   { UpdateState::idle };
    std::atomic<bool>          maqamDataLoading { false };  // true while maqam list is being fetched
    std::atomic<bool>          loadingFromCache { false };   // true while lazy-loading from disk

    /** Retry the last failed network operation. */
    void retryLastFailedFetch();

    /** Whether maqam list is cached for a given system+startingNote. */
    bool hasCachedMaqamList (const juce::String& systemId, const juce::String& startingNote) const;

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

    // Set by parameterChanged (DAW automation / MIDI CC) on ref_freq changes.
    // Editor timer picks this up at 30Hz to throttle UI updates.
    std::atomic<bool> refFreqAutomationDirty { false };

    // Set by parameterChanged on slot_N changes (DAW automation / MIDI CC).
    // Editor timer picks this up at 30Hz. Bitmask: bit i = chromatic index i dirty.
    std::atomic<uint16_t> slotAutomationDirtyMask { 0 };

    // ── MIDI note → preset triggering (MIDI Learn) ─────────────────────────────
    // Each preset can be mapped to a specific MIDI note (-1 = unmapped)
    // midiPresetChannel: 0 = any channel, 1-16 = specific channel
    std::array<std::atomic<int>, kNumMaqamPresets> midiPresetNotes;  // Note per preset, -1 = unmapped
    std::atomic<int> midiPresetChannel { 0 };          // 0 = any channel
    std::atomic<int> pendingMidiPreset { -1 };         // Set by MIDI callback, consumed by editor timer
    std::atomic<int> midiLearnTargetPreset { -1 };     // -1 = not learning, 0-7 = learning for preset

    void setMidiPresetNote (int presetIdx, int midiNote);
    int  getMidiPresetNote (int presetIdx) const;
    void setMidiPresetChannel (int channel);
    int  getMidiPresetChannel() const { return midiPresetChannel.load(); }
    int  consumePendingMidiPreset();  // Returns preset index (0-7) or -1 if none

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
    bool              isMidiPresetDeviceOpen() const;
    /** Check if the current MIDI device is still available; close stale connection if not. */
    void              recheckMidiPresetDevice();

    // MidiInputCallback override
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    // ── APVTS for DAW automation / MIDI mapping ───────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Parameter pointers for quick access (non-owning)
    std::array<juce::AudioParameterFloat*, kNumSlotParams> slotParams {};
    juce::AudioParameterChoice* presetParam = nullptr;
    juce::AudioParameterFloat*  refFreqParam = nullptr;

    // ── APVTS Listener ───────────────────────────────────────────────────────
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // ── Gesture marking for DAW automation recording ─────────────────────────
    void beginSliderGesture (int chromaticIndex);
    void endSliderGesture   (int chromaticIndex);
    void beginPresetGesture();
    void endPresetGesture();

    // ── File-based state save/load (user-managed .tanghim files) ───────────
    juce::String buildStateJson() const;
    void restoreStateFromJson (const juce::String& json);

    // ── Disk persistence (public for editor access) ──────────────────────────
    void saveSettingsToDisk() const;
    void clearCache();

private:
    // ── Reference frequency state ─────────────────────────────────────────────
    double       referenceCentsOffset     = 0.0;
    int          referenceNoteMidi        = 60;   // MIDI note of tonic in octave 1
    juce::String referenceNoteDisplayName;        // PAO display name (e.g. "yegāh")

    // ── Core state ────────────────────────────────────────────────────────────
    juce::String              currentSystemId;
    juce::String              currentStartingNote;
    ActiveTuningState         activeTuningState;
    std::array<MaqamPreset, kNumMaqamPresets> presets;
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

    // ── Direct MIDI device input for preset triggering ──────────────────────
    std::unique_ptr<juce::MidiInput> midiPresetInput;
    juce::String                     midiPresetDeviceName;  // Empty = disabled
    juce::String                     midiPresetDeviceId;    // OS-unique identifier for reliable matching

    // ── Lifetime guard (must be declared before apiClient so it outlives it) ─
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // ── Subsystems ────────────────────────────────────────────────────────────
    DiArMaqArClient  apiClient;
    ApiDataCache     dataCache;
    DataUpdateChecker updateChecker;
    TuningEngine     tuningEngine;
    TriangleOscillator oscillator;

    // ── Heptatonic mapping state (audio thread reads, message thread writes) ──
    std::array<std::atomic<int>, 12>  heptMap;             // input chromatic → signed semitone delta
    std::array<std::atomic<int>, 128> activeHeptNotes;     // inputNote → remappedNote for held notes (-1 = inactive)

    // ── Pitch bend wheel tracking (audio thread only) ───────────────────────
    int currentPitchBend = 8192;                           // 14-bit, center = 8192
    static constexpr double kPitchBendRangeSt = 2.0;      // ±2 semitones

    // ── Audio-thread slot automation polling ────────────────────────────────
    // Cached APVTS slot values from last processBlock — allows detection of
    // automation changes at buffer rate (vs message-thread parameterChanged).
    std::array<float, 12> lastAudioSlotValues {};
    void pollSlotAutomation();

    std::atomic<bool> autoDataUpdateCheckDone { false };
    void markAutoDataUpdateCheckCompleted() noexcept
    {
        autoDataUpdateCheckDone.store (true, std::memory_order_relaxed);
    }

    // ── Internal helpers ──────────────────────────────────────────────────────
    void rebuildTuningStateFromCache();
    void fetchMaqamListIfNeeded();
    void backgroundPreloadRemainingNotes (const juce::String& systemId, const juce::String& loadedNote);
    void backgroundPreloadMaqamLists (const juce::String& systemId,
                                       const juce::String& loadedNote,
                                       const std::vector<juce::String>& otherNotes);
    void applyDegreeIpnAndSolfege (const MaqamDegrees& degrees);
    void applyMaqamDegrees (const MaqamDegrees& degrees);
    void notifyTuningChanged();
    juce::String buildScaleName() const;

    // ── Heptatonic MTS-ESP broadcast helpers ────────────────────────────────
    /** Calls updateTuning + hept remap if needed. Use instead of direct updateTuning. */
    void updateTuningAndBroadcast();
    /** Build remapped freq table from heptMap deltas and broadcast to MTS-ESP. */
    void rebroadcastHeptMts();

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

    // ── Retry state for failed fetches ───────────────────────────────────────
    std::function<void()> lastFailedFetch;

    // ── Disk persistence ─────────────────────────────────────────────────────
    void loadSettingsFromDisk();
    void savePresetsToDisk() const;
    void loadPresetsFromDisk();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TanghimProcessor)
};
