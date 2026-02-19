#pragma once
#include <juce_core/juce_core.h>

/**
 * Metadata for a single tuning system from the DiArMaqAr API.
 * Pitch class data (notes, variants) is loaded separately and cached.
 */
struct TuningSystem
{
    juce::String id;                         // URL-safe id, e.g. "ibnsina_1037"
    juce::String displayName;                // e.g. "Ibn Sīnā (1037) 7-Fret Oud 17-Tone"
    juce::String shortName;                  // e.g. "Ibn Sīnā (1037)"
    int          year                 = 0;
    int          pitchClassesPerOctave = 12;
    double       referenceFrequency   = 440.0; // Hz for the reference pitch

    // Available starting notes for this system (e.g. "ushayran", "yegah")
    juce::StringArray startingNoteIds;
    juce::StringArray startingNoteDisplayNames;

    // API metadata
    juce::String version;                    // ISO 8601 for update detection

    bool isValid() const { return id.isNotEmpty(); }
};
