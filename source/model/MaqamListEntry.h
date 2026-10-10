#pragma once
#include <juce_core/juce_core.h>
#include <vector>

/**
 * Ascending/descending PAO note name arrays for a maqam or transposition.
 * These are noteName idNames (e.g. "rast", "dugah", "segah") that map
 * to pitch class variants already loaded from the tuning system.
 */
struct MaqamDegrees
{
    std::vector<juce::String> ascending;
    std::vector<juce::String> descending;
    std::vector<juce::String> ascendingEnglishNames;  // e.g., "Db3", "E-b3" (parallel to ascending)
    std::vector<juce::String> ascendingSolfeges;       // e.g., "Reb3", "Mi-b3" (parallel to ascending)

    // 12-EDO reference per degree, supplied by the API as a MATCHED PAIR: the key the
    // degree is spelled against, and its pitch expressed relative to that same key.
    // These are maqam-context values and can differ from the tuning system's global
    // pitch-class data (nim_ajam is globally "A / 57 +90.2" but "Bb / 58 -9.8" in
    // bayyat al-nawa). Never mix one with the other — see applyMaqamDegrees().
    std::vector<juce::String> ascendingIpnRefs;        // e.g., "Bb"  (parallel to ascending)
    std::vector<int>          ascendingMidiNotes;      // e.g., 58    (parallel to ascending)
    std::vector<double>       ascendingCentsDeviations;// e.g., -9.8  (relative to ascendingMidiNotes)
};

/**
 * A single transposition of a maqam, with its tonic and scale degrees.
 */
struct MaqamTransposition
{
    juce::String tonicId;        // "chahargah"
    juce::String tonicDisplay;   // "chahārgāh"
    MaqamDegrees degrees;
};

/**
 * A single entry from:
 *   GET /tuning-systems/{id}/{startingNote}/maqamat?includeMaqamDegrees=true&includeTranspositions=true
 *
 * Represents a maqam available in a specific tuning system + starting note,
 * with its scale degrees and available transpositions.
 */
struct MaqamListEntry
{
    juce::String maqamId;        // "maqam_rast"
    juce::String maqamDisplay;   // "maqām rāst"
    juce::String familyId;       // "rast"
    juce::String familyDisplay;  // "rāst"
    juce::String tonicId;        // "rast"
    juce::String tonicDisplay;   // "rāst"

    MaqamDegrees degrees;                           // base maqam ascending/descending
    std::vector<MaqamTransposition> transpositions;  // available transpositions
};
