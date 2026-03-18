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
