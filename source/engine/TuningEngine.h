#pragma once
#include "../model/ActiveTuningState.h"
#include "MtsEspTransmitter.h"
#include "MpePitchBendProcessor.h"
#include "MonoPitchBendProcessor.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <array>

enum class OutputMode
{
    MtsEsp,       ///< MTS-ESP transmitter (MIDI passthrough, tuning via shared memory)
    Mpe,          ///< MPE per-note pitch bend (channels 2-16)
    MonoPitchBend ///< 14-bit monophonic pitch bend (inserts bend before each note)
};

/**
 * Central tuning coordinator.
 *
 * Owns the 128-note frequency and cents-deviation tables, the MTS-ESP transmitter,
 * and the MPE/pitch-bend processors. Called from processBlock() to route MIDI
 * through the active output mode.
 *
 * Thread safety:
 *   updateTuning() is called on the message thread (from GUI/preset changes).
 *   processMidi()  is called on the audio thread.
 *   Tables are updated atomically via a CriticalSection.
 */
class TuningEngine
{
public:
    TuningEngine();
    ~TuningEngine();

    // ── Output mode ───────────────────────────────────────────────────────────
    void       setOutputMode (OutputMode mode);
    OutputMode getOutputMode() const;

    // MTS-ESP status
    bool isMtsTransmitter()  const;
    int  mtsNumReceivers()   const;

    // Pitch bend ranges (MPE and mono are independent)
    void setMpePitchBendRange  (int semitones);
    void setMonoPitchBendRange (int semitones);
    int  getMpePitchBendRange()  const;
    int  getMonoPitchBendRange() const;

    // ── Tuning update ─────────────────────────────────────────────────────────
    /**
     * Rebuild the frequency and cents-deviation tables from the given
     * tuning state and push the new tuning to the active output mode.
     * Call this whenever sliders change or a preset is loaded.
     * Safe to call from the message thread.
     */
    void updateTuning (const ActiveTuningState& state,
                       const juce::String& scaleName = {});

    /** Push the current tuning tables to MTS-ESP (call after mode switch). */
    void pushCurrentTuningToMts();

    // ── Audio-thread processing ───────────────────────────────────────────────
    /**
     * Process a MIDI buffer in processBlock().
     * In MTS-ESP mode: MIDI passes through unchanged.
     * In MPE mode:     notes are re-routed to member channels with pitch bend.
     * In mono PB mode: pitch bend is inserted before each Note On.
     */
    void processMidi (juce::MidiBuffer& midiInOut, int numSamples);

    // ── Current tuning data (for display / serialisation) ────────────────────
    double getFrequencyForMidiNote  (int midiNote) const;
    double getCentsDeviationForMidi (int midiNote) const;
    const std::array<double, 128>& getFrequencyTable()    const;
    const std::array<double, 128>& getCentsDeviationTable() const;

private:
    OutputMode currentMode { OutputMode::MtsEsp };

    // Tuning tables — written on message thread, read on audio thread
    mutable juce::CriticalSection tuningLock;
    std::array<double, 128> freqTable;
    std::array<double, 128> centsTable;

    std::unique_ptr<MtsEspTransmitter>     mtsEsp;
    std::unique_ptr<MpePitchBendProcessor> mpe;
    std::unique_ptr<MonoPitchBendProcessor> monoPb;

    bool mpeZoneConfigSent = false;
};
