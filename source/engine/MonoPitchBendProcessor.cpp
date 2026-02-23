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
    noteStack.clear();
    userPitchBend = 8192;
    activeCentsDeviation = 0.0;
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
            const int note = msg.getNoteNumber();

            // Force monophony: turn off the sounding note
            if (activeNote >= 0 && activeNote != note)
                out.addEvent (juce::MidiMessage::noteOff (activeChannel, activeNote), pos);

            // Add to stack (remove first if re-triggered)
            noteStack.erase (std::remove (noteStack.begin(), noteStack.end(), note), noteStack.end());
            noteStack.push_back (note);

            const double dev = (note >= 0 && note < 128) ? centsDeviationTable[(size_t) note] : 0.0;
            activeCentsDeviation = dev;

            // Combine microtuning offset with current user PB wheel position
            const int microBend = bendValue (dev, pbRange);
            const int userOffset = userPitchBend - 8192;
            const int combined = static_cast<int> (std::clamp (microBend + userOffset, 0, 16383));

            // Insert pitch bend then the note
            out.addEvent (juce::MidiMessage::pitchWheel (msg.getChannel(), combined), pos);
            out.addEvent (msg, pos);

            activeNote    = note;
            activeChannel = msg.getChannel();
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();

            // Remove from stack
            noteStack.erase (std::remove (noteStack.begin(), noteStack.end(), note), noteStack.end());

            if (note == activeNote)
            {
                out.addEvent (msg, pos);

                if (! noteStack.empty())
                {
                    // Recall previously held note
                    const int prev = noteStack.back();
                    const double dev = (prev >= 0 && prev < 128) ? centsDeviationTable[(size_t) prev] : 0.0;
                    activeCentsDeviation = dev;

                    const int microBend = bendValue (dev, pbRange);
                    const int userOffset = userPitchBend - 8192;
                    const int combined = static_cast<int> (std::clamp (microBend + userOffset, 0, 16383));

                    out.addEvent (juce::MidiMessage::pitchWheel (activeChannel, combined), pos);
                    out.addEvent (juce::MidiMessage::noteOn (activeChannel, prev, (juce::uint8) 100), pos);
                    activeNote = prev;
                }
                else
                {
                    activeNote = -1;
                }
            }
            // Swallowed: Note Off for a note already cut from stack
        }
        else if (msg.isPitchWheel())
        {
            // Intercept PB wheel: combine with microtuning offset
            userPitchBend = msg.getPitchWheelValue();
            const int microBend = bendValue (activeCentsDeviation, pbRange);
            const int userOffset = userPitchBend - 8192;
            const int combined = static_cast<int> (std::clamp (microBend + userOffset, 0, 16383));
            out.addEvent (juce::MidiMessage::pitchWheel (msg.getChannel(), combined), pos);
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
