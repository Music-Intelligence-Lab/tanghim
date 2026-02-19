#pragma once
#include "../model/PitchClass.h"
#include "../model/TuningSystem.h"
#include "../model/MaqamListEntry.h"
#include <juce_core/juce_core.h>
#include <vector>

/**
 * Pure static functions that parse raw juce::var JSON responses from the
 * DiArMaqAr API into strongly-typed model structs.
 *
 * All functions are stateless and testable without a running plugin.
 */
class ApiResponseParser
{
public:
    // ── Tuning systems list ───────────────────────────────────────────────────
    /** Parse GET /tuning-systems response. */
    static std::vector<TuningSystem> parseTuningSystemsList (const juce::var& json);

    // ── Pitch classes ─────────────────────────────────────────────────────────
    /**
     * Parse GET /tuning-systems/{id}/{note}/pitch-classes?pitchClassDataType=all
     * Returns all pitch classes enriched with midiNoteDeviation, englishName,
     * cents, frequency etc.
     */
    static std::vector<PitchClass> parsePitchClasses (const juce::var& json);

    // ── Maqam list & detail ────────────────────────────────────────────────────
    /**
     * Parse GET /tuning-systems/{id}/{startingNote}/maqamat response.
     * Returns a list of maqam entries with family and tonic info.
     */
    static std::vector<MaqamListEntry> parseMaqamList (const juce::var& json);

    /**
     * Parse GET /maqamat/{idName}?...&pitchClassDataType=midiNoteDeviation
     * Returns the ascending pitch classes from the maqam detail response.
     */
    static std::vector<PitchClass> parseMaqamDetail (const juce::var& json);

    // ── Helpers ───────────────────────────────────────────────────────────────
    /**
     * Parse a "midiNoteDeviation" string, e.g. "48 -5.87"
     * Returns false if the string cannot be parsed.
     */
    static bool parseMidiNoteDeviation (const juce::String& s,
                                        int&    midiNoteOut,
                                        double& centsOut);

    /**
     * Build ChromaticNoteVariants (one per chromatic position) from a flat
     * list of pitch classes. Groups pitches by IPN reference.
     * Variants within each slot are sorted by ascending cents value.
     */
    static std::array<std::vector<PitchClass>, 12>
        buildVariantsPerSlot (const std::vector<PitchClass>& pitchClasses);

private:
    static PitchClass parseSinglePitchClass (const juce::var& obj);
    static TuningSystem parseSingleTuningSystem (const juce::var& obj);
    static MaqamDegrees parseMaqamDegrees (const juce::var& obj);
};
