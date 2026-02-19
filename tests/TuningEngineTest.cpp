#include "../source/model/ActiveTuningState.h"
#include <iostream>
#include <cmath>

static int passed = 0, failed = 0;
#define TEST(name) std::cout << "  " << name << " ... "; int _pf = failed;
#define CHECK(e) do { if (!(e)) { std::cout << "\n    FAIL: " #e; ++failed; } } while(0)
#define CHECK_NEAR(a,b,tol) CHECK(std::abs((a)-(b)) < (tol))
#define END_TEST do { if (failed==_pf) { std::cout << "OK\n"; ++passed; } else std::cout << "\n"; } while(0)

int main()
{
    std::cout << "TuningEngine / ActiveTuningState tests\n";

    // ── 12-EDO frequency table (no deviations) ─────────────────────────────
    {
        TEST("12-EDO: A4 (MIDI 69) = 440 Hz")
        ActiveTuningState state;
        // Leave all slots empty → deviations = 0 → 12-EDO
        const auto freqs = state.buildFrequencyTable();
        CHECK_NEAR (freqs[69], 440.0, 0.001);
        END_TEST;
    }
    {
        TEST("12-EDO: C4 (MIDI 60) ≈ 261.626 Hz")
        ActiveTuningState state;
        const auto freqs = state.buildFrequencyTable();
        CHECK_NEAR (freqs[60], 261.626, 0.01);
        END_TEST;
    }
    {
        TEST("12-EDO: A5 (MIDI 81) = 880 Hz")
        ActiveTuningState state;
        const auto freqs = state.buildFrequencyTable();
        CHECK_NEAR (freqs[81], 880.0, 0.001);
        END_TEST;
    }

    // ── Cents deviation applied ────────────────────────────────────────────
    {
        TEST("+50 cents deviation on all notes shifts frequency up")
        ActiveTuningState state;
        for (int i = 0; i < 12; ++i)
        {
            PitchClass pc;
            pc.noteName          = "test";
            pc.midiNoteNumber    = i;
            pc.midiCentsDeviation = 50.0;
            pc.ipnReference      = kChromaticIpnRefs[i];
            state.slots[(size_t) i].variants.push_back (pc);
        }
        const auto freqs = state.buildFrequencyTable();
        const double expected = 440.0 * std::pow (2.0, ((69 * 100.0) + 50.0 - 69 * 100.0) / 1200.0);
        // A4 + 50 cents
        const double a4shifted = 440.0 * std::pow (2.0, 50.0 / 1200.0);
        CHECK_NEAR (freqs[69], a4shifted, 0.01);
        END_TEST;
    }

    // ── Cents deviation table ──────────────────────────────────────────────
    {
        TEST("centsDeviationTable: -5.87 on all E positions (MIDI 4, 16, 28 ...)")
        ActiveTuningState state;
        PitchClass pc;
        pc.noteName           = "segah";
        pc.midiNoteNumber     = 4;
        pc.midiCentsDeviation = -5.87;
        pc.ipnReference       = "E";
        state.slots[4].variants.push_back (pc); // slot 4 = E

        const auto cents = state.buildCentsDeviationTable();
        // All E notes (4, 16, 28, 40, 52, 64, 76, 88, 100, 112, 124) should have -5.87
        for (int midi = 4; midi < 128; midi += 12)
            CHECK_NEAR (cents[(size_t) midi], -5.87, 0.001);
        // C notes (0, 12, 24 ...) should have 0.0
        for (int midi = 0; midi < 128; midi += 12)
            CHECK_NEAR (cents[(size_t) midi], 0.0, 0.001);
        END_TEST;
    }

    // ── Slider locking ─────────────────────────────────────────────────────
    {
        TEST("slot with 1 variant is locked")
        ChromaticNoteVariants slot;
        slot.variants.push_back (PitchClass{});
        CHECK (slot.isLocked());
        END_TEST;
    }
    {
        TEST("slot with 2 variants is not locked")
        ChromaticNoteVariants slot;
        slot.variants.push_back (PitchClass{});
        slot.variants.push_back (PitchClass{});
        CHECK (! slot.isLocked());
        END_TEST;
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
