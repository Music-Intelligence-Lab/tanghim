#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

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

    /** Send Note Off + PB reset for the active note (used on mode switch). */
    void allNotesOff (juce::MidiBuffer& out, int samplePosition = 0);

    void setPitchBendRange (int semitones) { pbRange = semitones; }
    int  getPitchBendRange() const         { return pbRange; }

    /** Returns true if a note is currently sounding. */
    bool hasActiveNotes() const { return activeNote >= 0; }

private:
    int pbRange;
    int activeNote = -1;    ///< Currently sounding MIDI note (-1 = none)
    int activeChannel = 1;  ///< Channel of the active note
    int userPitchBend = 8192;        ///< Incoming PB wheel value (center = 8192)
    double activeCentsDeviation = 0.0; ///< Cents deviation of the active/last note
    std::vector<int> noteStack;      ///< Held notes in press order (last = sounding)

    static int bendValue (double centsDeviation, int rangeSemitones);
};
