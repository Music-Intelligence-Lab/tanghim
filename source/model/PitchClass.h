#pragma once
#include <juce_core/juce_core.h>

/**
 * A single pitch in a tuning system, enriched with all data needed for
 * MIDI microtuning. Corresponds to the DiArMaqAr API PitchClass model.
 *
 * Arabic Musicological Logic:
 *   englishName is the primary source of truth for IPN reference assignment.
 *   Microtonal modifiers (e.g. "-b" in "E-b3") indicate what a pitch is a
 *   variant OF, not its proximity to a 12-EDO semitone.
 *   "E-b3" → variant of E (not Eb).
 */
struct PitchClass
{
    // ── Identity ──────────────────────────────────────────────────────────────
    int    pitchClassIndex = 0;          // 0-based position within one octave of the tuning system
    int    octave          = 0;          // Octave number (MIDI convention)

    // ── Names ─────────────────────────────────────────────────────────────────
    juce::String noteName;               // URL-safe id, e.g. "segah", "buselik_ushshaq"
    juce::String noteNameDisplay;        // With diacritics, e.g. "segāh", "būselīk/ʿushshāq"
    juce::String englishName;            // IPN + microtonal, e.g. "E-b3", "D#3", "C3"
    juce::String solfege;               // Solfège notation, e.g. "Mi -b3", "Do 2", "Sol 1"
    juce::String abjadName;             // Arabic abjad notation

    // ── Tuning data ───────────────────────────────────────────────────────────
    juce::String fraction;               // Frequency ratio, e.g. "13/9"
    double       cents            = 0.0; // Cents from tonic
    double       frequency        = 0.0; // Absolute Hz

    // ── MIDI representation ───────────────────────────────────────────────────
    // Parsed from the API's "midiNoteDeviation" string, e.g. "52 -63.4"
    int    midiNoteNumber     = 60;      // 12-EDO reference MIDI note
    double midiCentsDeviation = 0.0;     // Deviation from that reference in cents

    // ── Computed IPN reference ────────────────────────────────────────────────
    // Derived from englishName using Arabic musicological logic.
    // "E-b3" → "E", "D#3" → "D#", "C3" → "C"
    juce::String ipnReference;           // One of: C C# D Eb E F F# G Ab A Bb B

    // ── API metadata ──────────────────────────────────────────────────────────
    juce::String version;                // ISO 8601 timestamp for update detection

    bool isValid() const { return noteName.isNotEmpty(); }

    /**
     * Parse a "midiNoteDeviation" string from the API (e.g. "52 -63.4")
     * into midiNoteNumber and midiCentsDeviation fields.
     */
    static bool parseMidiNoteDeviation (const juce::String& s, int& noteOut, double& centsOut)
    {
        const int spaceIdx = s.indexOfChar (' ');
        if (spaceIdx < 0) return false;
        noteOut  = s.substring (0, spaceIdx).getIntValue();
        centsOut = s.substring (spaceIdx + 1).getDoubleValue();
        return true;
    }

    /**
     * Derive the IPN chromatic reference from an englishName string.
     * Logic mirrors calculateIpnReferenceMidiNote.ts from DiArMaqAr.
     *
     * Examples:
     *   "E-b3"  → "E"   (microtonal modifier stripped, base note preserved)
     *   "D#3"   → "D#"  (standard sharp kept)
     *   "Bb3"   → "Bb"  (standard flat kept)
     *   "C3"    → "C"
     */
    static juce::String ipnReferenceFromEnglishName (const juce::String& englishName)
    {
        if (englishName.isEmpty()) return {};

        // Match: base note (A-G), optional standard accidental (#/b),
        //        optional microtonal modifier, digits
        // The microtonal modifier is any non-digit chars after the accidental
        // e.g. "-b", "-#", "+" — we strip these.
        juce::String s = englishName.trim();

        // Extract base note letter
        if (s.isEmpty()) return {};
        juce::String base = s.substring (0, 1).toUpperCase();

        // Check for standard accidental immediately following the base letter
        juce::String accidental;
        if (s.length() > 1)
        {
            const auto c = s[1];
            if (c == '#') accidental = "#";
            else if (c == 'b' && s.length() > 2 && juce::CharacterFunctions::isDigit (s[2]))
                accidental = "b";   // "Bb3" — 'b' followed by digit
            // If 'b' is followed by non-digit it's a microtonal modifier → ignore
        }

        return base + accidental;
    }
};

/** Ordered list of the 12 chromatic IPN references (flats for Eb, Ab, Bb). */
static constexpr const char* kChromaticIpnRefs[12] = {
    "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
};

/** Return 0-11 for a recognised IPN reference, or -1 if unknown.
 *  Accepts both sharp and flat equivalents (e.g. "D#" → 3, "Eb" → 3). */
inline int chromaticIndexForIpnRef (const juce::String& ref)
{
    for (int i = 0; i < 12; ++i)
        if (ref == kChromaticIpnRefs[i]) return i;

    // Accept enharmonic equivalents (sharps ↔ flats)
    if (ref == "Db") return 1;
    if (ref == "D#") return 3;
    if (ref == "Gb") return 6;
    if (ref == "G#") return 8;
    if (ref == "A#") return 10;

    return -1;
}
