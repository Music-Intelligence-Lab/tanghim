#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

/**
 * Converts incoming MIDI (channel 1) to MPE by assigning each note to a
 * dedicated MPE member channel (2-16) and prefixing it with a 14-bit
 * per-note pitch bend.
 *
 * MPE lower zone layout:
 *   Channel 1  — Manager channel (global CC, aftertouch, program change)
 *   Channels 2-16 — Member channels (one per active note)
 *
 * Pitch bend range is 48 semitones by default, which covers the largest
 * deviations in Arabic tuning systems (e.g. ~+107 cents for nīm ḥijāz).
 * The receiving synth must be configured with the same pitch bend range.
 */
class MpePitchBendProcessor
{
public:
    explicit MpePitchBendProcessor (int pitchBendRangeSemitones = 48);

    /** Send MPE zone configuration RPN on the manager channel. */
    void sendMpeZoneConfig (juce::MidiBuffer& out, int samplePosition = 0) const;

    /**
     * Process a MIDI buffer: intercept Note On/Off on any channel and
     * re-emit them on MPE member channels with appropriate pitch bend.
     * All other messages are forwarded on channel 1.
     */
    void process (const juce::MidiBuffer& in,
                  juce::MidiBuffer&       out,
                  const std::array<double, 128>& centsDeviationTable,
                  int numSamples);

    void setPitchBendRange (int semitones) { pbRange = semitones; }
    int  getPitchBendRange() const         { return pbRange; }

private:
    int pbRange;

    struct ChannelState
    {
        int  noteNumber = -1;
        bool active     = false;
    };
    // Channels 2-16 (indices 0-14 map to MIDI channels 2-16)
    std::array<ChannelState, 15> channels {};
    int nextChannel = 0; // round-robin index into channels[]

    int  allocateChannel (int noteNumber);
    void releaseChannel  (int noteNumber);
    int  findChannelForNote (int noteNumber) const;

    /** 14-bit pitch bend value for a cents deviation and pitch bend range. */
    static int bendValue (double centsDeviation, int rangeSemitones);
};
