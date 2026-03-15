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

    /**
     * Fast path: reapply only the reference frequency offset to the cached
     * base frequency table. Avoids rebuilding the entire table when only
     * the concert pitch knob changed.
     */
    void updateReferenceOffset (double referenceCentsOffset);

    /**
     * Fast path: update only one chromatic slot (and its octave copies)
     * in the cached base/freq/cents tables. Avoids a full 128-note rebuild
     * when DAW automation or MIDI CC changes a single slot_N param.
     */
    void updateSlotTuning (int chromaticIndex, double centsOffset,
                           double referenceCentsOffset);

    /**
     * Fast path: update a single MIDI note's tuning.
     * Used for per-note cents overrides (Shift+drag).
     */
    void updateNoteTuning (int midiNote, double centsOffset,
                           double referenceCentsOffset);

    // ── MTS-ESP broadcast (without modifying internal tables) ────────────────
    /** Broadcast a frequency table to MTS-ESP without updating internal state. */
    void broadcastMtsTable (const std::array<double, 128>& freqs);

    /** Re-broadcast the current internal frequency table to MTS-ESP. */
    void rebroadcastCurrentTuning();

    // ── Current tuning data (for display / serialisation) ────────────────────
    double getFrequencyForMidiNote  (int midiNote) const;
    double getCentsDeviationForMidi (int midiNote) const;
    const std::array<double, 128>& getFrequencyTable()    const;
    const std::array<double, 128>& getCentsDeviationTable() const;

    /** Copy frequency table in one lock acquisition (audio-thread friendly). */
    void snapshotFrequencyTable (std::array<double, 128>& dest) const;

private:
    // Tuning tables — written on message thread, read on audio thread
    mutable juce::CriticalSection tuningLock;
    std::array<double, 128> freqTable;
    std::array<double, 128> baseFreqTable;  // before reference offset — for fast ref-only updates
    std::array<double, 128> centsTable;

    std::unique_ptr<MtsEspTransmitter> mtsEsp;
};
