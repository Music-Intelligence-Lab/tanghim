#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

/**
 * Inserts a 14-bit monophonic pitch bend message immediately before each
 * Note On, using the cents deviation for that note.
 *
 * Forces monophony: when a new note arrives while another is held, the
 * previous note is automatically turned off before the new one sounds.
 * This ensures the single-channel pitch bend always matches the sounding note.
 *
 * The receiving synth's pitch bend range must match pitchBendRangeSemitones
 * (default 2 = +/-200 cents, the most common synth default).
 */
class MonoPitchBendProcessor
{
public:
    explicit MonoPitchBendProcessor (int pitchBendRangeSemitones = 2);

    void process (const juce::MidiBuffer&          in,
                  juce::MidiBuffer&                 out,
                  const std::array<double, 128>&    centsDeviationTable);

    void setPitchBendRange (int semitones) { pbRange = semitones; }
    int  getPitchBendRange() const         { return pbRange; }

private:
    int pbRange;
    int activeNote = -1;    ///< Currently sounding MIDI note (-1 = none)
    int activeChannel = 1;  ///< Channel of the active note

    static int bendValue (double centsDeviation, int rangeSemitones);
};
