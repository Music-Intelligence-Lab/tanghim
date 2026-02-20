#include "MonoPitchBendProcessor.h"
#include <cmath>
#include <algorithm>

MonoPitchBendProcessor::MonoPitchBendProcessor (int pitchBendRangeSemitones)
    : pbRange (pitchBendRangeSemitones) {}

void MonoPitchBendProcessor::allNotesOff (juce::MidiBuffer& out, int samplePos)
{
    if (activeNote >= 0)
    {
        out.addEvent (juce::MidiMessage::noteOff (activeChannel, activeNote), samplePos);
        out.addEvent (juce::MidiMessage::pitchWheel (activeChannel, 8192), samplePos);
        activeNote = -1;
    }
}

void MonoPitchBendProcessor::process (const juce::MidiBuffer&       in,
                                       juce::MidiBuffer&              out,
                                       const std::array<double, 128>& centsDeviationTable)
{
    for (const auto meta : in)
    {
        const auto msg = meta.getMessage();
        const int  pos = meta.samplePosition;

        if (msg.isNoteOn())
        {
            // Force monophony: turn off the previous note if one is held
            if (activeNote >= 0)
                out.addEvent (juce::MidiMessage::noteOff (activeChannel, activeNote), pos);

            const int note = msg.getNoteNumber();
            const double dev = (note >= 0 && note < 128) ? centsDeviationTable[(size_t) note] : 0.0;
            const int bv = bendValue (dev, pbRange);

            // Insert pitch bend then the note
            out.addEvent (juce::MidiMessage::pitchWheel (msg.getChannel(), bv), pos);
            out.addEvent (msg, pos);

            activeNote    = note;
            activeChannel = msg.getChannel();
        }
        else if (msg.isNoteOff())
        {
            // Only pass through the Note Off if it matches the active note
            if (msg.getNoteNumber() == activeNote)
            {
                out.addEvent (msg, pos);
                activeNote = -1;
            }
            // Swallowed: Note Off for a note we already cut
        }
        else
        {
            out.addEvent (msg, pos);
        }
    }
}

int MonoPitchBendProcessor::bendValue (double centsDeviation, int rangeSemitones)
{
    if (rangeSemitones <= 0) return 8192;
    if (std::isnan (centsDeviation) || std::isinf (centsDeviation)) return 8192;
    const double ratio = centsDeviation / (rangeSemitones * 100.0);
    const double raw = 8192.0 + std::round (ratio * 8192.0);
    return static_cast<int> (std::clamp (raw, 0.0, 16383.0));
}
