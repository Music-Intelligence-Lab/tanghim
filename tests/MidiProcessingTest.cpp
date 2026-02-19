#include "../source/engine/MpePitchBendProcessor.h"
#include "../source/engine/MonoPitchBendProcessor.h"
#include <iostream>
#include <cmath>

static int passed = 0, failed = 0;
#define TEST(name) std::cout << "  " << name << " ... "; int _pf = failed;
#define CHECK(e) do { if (!(e)) { std::cout << "\n    FAIL: " #e; ++failed; } } while(0)
#define CHECK_NEAR(a,b,tol) CHECK(std::abs((double)(a)-(double)(b)) < (tol))
#define END_TEST do { if (failed==_pf) { std::cout << "OK\n"; ++passed; } else std::cout << "\n"; } while(0)

// Build a table with one known deviation
static std::array<double, 128> makeDeviationTable (int midiNote, double cents)
{
    std::array<double, 128> t; t.fill (0.0);
    for (int i = midiNote % 12; i < 128; i += 12)
        t[(size_t) i] = cents;
    return t;
}

int main()
{
    std::cout << "MIDI processing tests\n";

    // ── MonoPitchBendProcessor ──────────────────────────────────────────────
    {
        TEST("MonoPB: 0 deviation → pitch bend = 8192 (centre)")
        MonoPitchBendProcessor proc (2);
        juce::MidiBuffer in, out;
        in.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
        std::array<double, 128> zeros; zeros.fill (0.0);
        proc.process (in, out, zeros);

        // First message should be a pitch bend at centre
        auto it = out.begin();
        CHECK (it != out.end());
        auto msg = (*it).getMessage();
        CHECK (msg.isPitchWheel());
        CHECK_NEAR (msg.getPitchWheelValue(), 8192, 1);
        END_TEST;
    }

    {
        TEST("MonoPB: +100 cents with 2-semitone range → bend = 12288")
        MonoPitchBendProcessor proc (2);
        juce::MidiBuffer in, out;
        in.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
        auto deviations = makeDeviationTable (69, +100.0);
        proc.process (in, out, deviations);

        auto it = out.begin();
        auto msg = (*it).getMessage();
        CHECK (msg.isPitchWheel());
        // +100 cents / (2 * 100 cents) * 8192 + 8192 = 12288
        CHECK_NEAR (msg.getPitchWheelValue(), 12288, 2);
        END_TEST;
    }

    {
        TEST("MonoPB: Note On is emitted after pitch bend")
        MonoPitchBendProcessor proc (2);
        juce::MidiBuffer in, out;
        in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 80), 0);
        std::array<double, 128> zeros; zeros.fill (0.0);
        proc.process (in, out, zeros);

        auto it = out.begin();
        ++it; // skip pitch bend
        CHECK (it != out.end());
        CHECK ((*it).getMessage().isNoteOn());
        CHECK ((*it).getMessage().getNoteNumber() == 60);
        END_TEST;
    }

    // ── MpePitchBendProcessor ───────────────────────────────────────────────
    {
        TEST("MPE: Note On is re-emitted on channel 2-16 (not ch 1)")
        MpePitchBendProcessor proc (48);
        juce::MidiBuffer in, out;
        in.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
        std::array<double, 128> zeros; zeros.fill (0.0);
        proc.process (in, out, zeros, 512);

        bool foundNoteOn = false;
        for (const auto& meta : out)
        {
            const auto& msg = meta.getMessage();
            if (msg.isNoteOn())
            {
                CHECK (msg.getChannel() >= 2 && msg.getChannel() <= 16);
                CHECK (msg.getNoteNumber() == 69);
                foundNoteOn = true;
            }
        }
        CHECK (foundNoteOn);
        END_TEST;
    }

    {
        TEST("MPE: Pitch bend precedes Note On for same sample position")
        MpePitchBendProcessor proc (48);
        juce::MidiBuffer in, out;
        in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        std::array<double, 128> zeros; zeros.fill (0.0);
        proc.process (in, out, zeros, 512);

        bool seenBend = false, bendBeforeNote = true;
        for (const auto& meta : out)
        {
            const auto& msg = meta.getMessage();
            if (msg.isPitchWheel() && msg.getChannel() >= 2) seenBend = true;
            if (msg.isNoteOn() && ! seenBend) bendBeforeNote = false;
        }
        CHECK (seenBend);
        CHECK (bendBeforeNote);
        END_TEST;
    }

    {
        TEST("MPE: Note Off on same channel as Note On")
        MpePitchBendProcessor proc (48);
        juce::MidiBuffer in, out;
        in.addEvent (juce::MidiMessage::noteOn  (1, 60, (juce::uint8) 100), 0);
        in.addEvent (juce::MidiMessage::noteOff (1, 60, (juce::uint8) 64),  10);
        std::array<double, 128> zeros; zeros.fill (0.0);
        proc.process (in, out, zeros, 512);

        int noteOnChannel = -1, noteOffChannel = -1;
        for (const auto& meta : out)
        {
            const auto& msg = meta.getMessage();
            if (msg.isNoteOn())  noteOnChannel  = msg.getChannel();
            if (msg.isNoteOff()) noteOffChannel = msg.getChannel();
        }
        CHECK (noteOnChannel >= 2);
        CHECK (noteOnChannel == noteOffChannel);
        END_TEST;
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
