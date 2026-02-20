#include "MpePitchBendProcessor.h"
#include <algorithm>
#include <cmath>

MpePitchBendProcessor::MpePitchBendProcessor (int pitchBendRangeSemitones)
    : pbRange (pitchBendRangeSemitones) {}

// ── MPE zone configuration ────────────────────────────────────────────────────

void MpePitchBendProcessor::sendMpeZoneConfig (juce::MidiBuffer& out, int samplePos) const
{
    // Set MPE lower zone: manager ch1, 15 member channels (2-16)
    // RPN 6 on channel 1: number of member channels
    // 1. Select RPN 6 (MSB=0, LSB=6)
    out.addEvent (juce::MidiMessage::controllerEvent (1, 101, 0),   samplePos); // RPN MSB
    out.addEvent (juce::MidiMessage::controllerEvent (1, 100, 6),   samplePos); // RPN LSB = 6
    // 2. Data entry: 15 member channels
    out.addEvent (juce::MidiMessage::controllerEvent (1, 6,  15),   samplePos); // Data Entry MSB
    out.addEvent (juce::MidiMessage::controllerEvent (1, 38, 0),    samplePos); // Data Entry LSB

    // Set pitch bend range for all member channels (RPN 0 on each)
    for (int ch = 2; ch <= 16; ++ch)
    {
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 101, 0),       samplePos);
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 100, 0),       samplePos);
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 6,  pbRange),  samplePos);
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 38, 0),        samplePos);
    }
}

// ── Flush all active notes ────────────────────────────────────────────────────

void MpePitchBendProcessor::allNotesOff (juce::MidiBuffer& out, int samplePos)
{
    for (int i = 0; i < 15; ++i)
    {
        if (channels[(size_t) i].active)
        {
            const int ch = i + 2;
            out.addEvent (juce::MidiMessage::noteOff (ch, channels[(size_t) i].noteNumber), samplePos);
            out.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), samplePos);
            channels[(size_t) i] = { -1, false };
        }
    }
    nextChannel = 0;
}

// ── Per-block processing ──────────────────────────────────────────────────────

void MpePitchBendProcessor::process (const juce::MidiBuffer&          in,
                                      juce::MidiBuffer&                 out,
                                      const std::array<double, 128>&    centsDeviationTable,
                                      int                               /*numSamples*/)
{
    for (const auto meta : in)
    {
        const auto msg = meta.getMessage();
        const int  pos = meta.samplePosition;

        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const int ch   = allocateChannel (note);
            const double dev = (note >= 0 && note < 128) ? centsDeviationTable[(size_t) note] : 0.0;

            // Send pitch bend on the allocated channel before the note
            const int bv = bendValue (dev, pbRange);
            out.addEvent (juce::MidiMessage::pitchWheel (ch, bv), pos);

            // Re-emit Note On on the MPE member channel
            out.addEvent (juce::MidiMessage::noteOn (ch, note, msg.getVelocity()), pos);
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            const int ch   = findChannelForNote (note);
            if (ch > 0)
            {
                out.addEvent (juce::MidiMessage::noteOff (ch, note, msg.getVelocity()), pos);
                releaseChannel (note);
                // Reset pitch bend to centre on released channel
                out.addEvent (juce::MidiMessage::pitchWheel (ch, 8192), pos);
            }
        }
        else
        {
            // Pass everything else through on channel 1 (manager channel)
            auto forwarded = msg;
            forwarded.setChannel (1);
            out.addEvent (forwarded, pos);
        }
    }
}

// ── Channel allocation ────────────────────────────────────────────────────────

int MpePitchBendProcessor::allocateChannel (int noteNumber)
{
    // Look for a free slot first
    for (int i = 0; i < 15; ++i)
    {
        int idx = (nextChannel + i) % 15;
        if (! channels[(size_t) idx].active)
        {
            channels[(size_t) idx] = { noteNumber, true };
            nextChannel = (idx + 1) % 15;
            return idx + 2; // MIDI channel (2-16)
        }
    }

    // All channels active — steal the round-robin next slot
    int idx = nextChannel;
    channels[(size_t) idx] = { noteNumber, true };
    nextChannel = (nextChannel + 1) % 15;
    return idx + 2;
}

void MpePitchBendProcessor::releaseChannel (int noteNumber)
{
    for (auto& ch : channels)
        if (ch.active && ch.noteNumber == noteNumber)
            { ch.active = false; ch.noteNumber = -1; return; }
}

int MpePitchBendProcessor::findChannelForNote (int noteNumber) const
{
    for (int i = 0; i < 15; ++i)
        if (channels[(size_t) i].active && channels[(size_t) i].noteNumber == noteNumber)
            return i + 2;
    return -1; // not found
}

int MpePitchBendProcessor::bendValue (double centsDeviation, int rangeSemitones)
{
    if (rangeSemitones <= 0) return 8192;
    if (std::isnan (centsDeviation) || std::isinf (centsDeviation)) return 8192;
    const double ratio = centsDeviation / (rangeSemitones * 100.0);
    const double raw = 8192.0 + std::round (ratio * 8192.0);
    return static_cast<int> (std::clamp (raw, 0.0, 16383.0));
}
