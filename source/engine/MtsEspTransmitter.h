#pragma once
#include <juce_core/juce_core.h>
#include <array>

/**
 * Thin RAII wrapper around the MTS-ESP Master C API (libMTSMaster.h).
 *
 * Only one MTS-ESP transmitter can exist per DAW session. Call canRegister()
 * before constructing to check availability. If registration fails (another
 * transmitter is already active), the wrapper is inert and isTransmitter() returns false.
 */
class MtsEspTransmitter
{
public:
    MtsEspTransmitter();
    ~MtsEspTransmitter();

    // Non-copyable
    MtsEspTransmitter (const MtsEspTransmitter&) = delete;
    MtsEspTransmitter& operator= (const MtsEspTransmitter&) = delete;

    /** True if another MTS-ESP transmitter is already registered in this session. */
    static bool hasExistingTransmitter();

    /** True if this instance is the registered transmitter. */
    bool isTransmitter() const { return registered; }

    /** Number of synths currently connected as MTS-ESP receivers. */
    int numReceivers() const;

    /**
     * Push a 128-note frequency table (Hz) to all connected receivers.
     * Has no effect if not the transmitter.
     */
    void setTuningTable (const std::array<double, 128>& frequenciesHz);

    /**
     * Set the scale name shown in MTS-ESP receiver UIs.
     * Has no effect if not the transmitter.
     */
    void setScaleName (const juce::String& name);

    /**
     * Filter a MIDI note from tuning (receivers treat it as untuned / 12-EDO).
     * Useful for percussion channels.
     */
    void filterNote (int midiNote, bool shouldFilter);

    /** Reset all note filters. */
    void clearNoteFilters();

private:
    bool registered = false;
};
