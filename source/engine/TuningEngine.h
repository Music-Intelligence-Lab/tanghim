#pragma once
#include "../model/ActiveTuningState.h"
#include "MtsEspTransmitter.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <array>

/**
 * Central tuning coordinator.
 *
 * Owns the 128-note frequency and cents-deviation tables and the MTS-ESP
 * transmitter. Broadcasts tuning via MTS-ESP shared memory; MIDI passes
 * through unchanged. Pitch bend output (MPE / mono) is handled exclusively
 * by the Receiver plugin.
 *
 * Thread safety:
 *   updateTuning() is called on the message thread (from GUI/preset changes).
 *   Tables are updated atomically via a CriticalSection.
 */
class TuningEngine
{
public:
    TuningEngine();
    ~TuningEngine();

    // MTS-ESP status
    bool isMtsTransmitter()  const;
    int  mtsNumReceivers()   const;

    // ── Tuning update ─────────────────────────────────────────────────────────
    /**
     * Rebuild the frequency and cents-deviation tables from the given
     * tuning state and push to MTS-ESP.
     * Call this whenever sliders change or a preset is loaded.
     * Safe to call from the message thread.
     */
    void updateTuning (const ActiveTuningState& state,
                       double referenceCentsOffset = 0.0,
                       const juce::String& scaleName = {});

    // ── Current tuning data (for display / serialisation) ────────────────────
    double getFrequencyForMidiNote  (int midiNote) const;
    double getCentsDeviationForMidi (int midiNote) const;
    const std::array<double, 128>& getFrequencyTable()    const;
    const std::array<double, 128>& getCentsDeviationTable() const;

private:
    // Tuning tables — written on message thread, read on audio thread
    mutable juce::CriticalSection tuningLock;
    std::array<double, 128> freqTable;
    std::array<double, 128> centsTable;

    std::unique_ptr<MtsEspTransmitter> mtsEsp;
};
