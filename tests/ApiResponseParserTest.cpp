#include "../source/api/ApiResponseParser.h"
#define CATCH_CONFIG_MAIN
#include <cassert>
#include <iostream>

// ── Minimal test harness (no external dependency) ─────────────────────────────
// These are compiled and run as a standalone executable.
// Replace with Catch2 once the build is verified.

static int passed = 0, failed = 0;

#define TEST(name) std::cout << "  " << name << " ... "; int _prevFailed = failed;
#define CHECK(expr) do { if (!(expr)) { std::cout << "\n    FAIL: " #expr; ++failed; } } while(0)
#define END_TEST do { if (failed == _prevFailed) { std::cout << "OK\n"; ++passed; } else std::cout << "\n"; } while(0)

int main()
{
    std::cout << "ApiResponseParser tests\n";

    // ── parseMidiNoteDeviation ─────────────────────────────────────────────
    {
        TEST("parseMidiNoteDeviation: '48 -5.87'")
        int note; double cents;
        bool ok = ApiResponseParser::parseMidiNoteDeviation ("48 -5.87", note, cents);
        CHECK (ok);
        CHECK (note == 48);
        CHECK (std::abs (cents - (-5.87)) < 0.001);
        END_TEST;
    }

    {
        TEST("parseMidiNoteDeviation: '52 +1.96'")
        int note; double cents;
        bool ok = ApiResponseParser::parseMidiNoteDeviation ("52 +1.96", note, cents);
        CHECK (ok);
        CHECK (note == 52);
        CHECK (std::abs (cents - 1.96) < 0.001);
        END_TEST;
    }

    {
        TEST("parseMidiNoteDeviation: '45 0.00'")
        int note; double cents;
        bool ok = ApiResponseParser::parseMidiNoteDeviation ("45 0.00", note, cents);
        CHECK (ok);
        CHECK (note == 45);
        CHECK (std::abs (cents) < 0.001);
        END_TEST;
    }

    {
        TEST("parseMidiNoteDeviation: empty string → false")
        int note; double cents;
        bool ok = ApiResponseParser::parseMidiNoteDeviation ("", note, cents);
        CHECK (! ok);
        END_TEST;
    }


    // ── chromaticIndexForIpnRef ────────────────────────────────────────────
    {
        TEST("chromaticIndexForIpnRef: C=0, C#=1, B=11")
        CHECK (chromaticIndexForIpnRef ("C")  == 0);
        CHECK (chromaticIndexForIpnRef ("C#") == 1);
        CHECK (chromaticIndexForIpnRef ("D")  == 2);
        CHECK (chromaticIndexForIpnRef ("E")  == 4);
        CHECK (chromaticIndexForIpnRef ("B")  == 11);
        CHECK (chromaticIndexForIpnRef ("X")  == -1);
        END_TEST;
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
