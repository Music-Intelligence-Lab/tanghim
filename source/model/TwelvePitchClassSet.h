#pragma once
#include "PitchClass.h"
#include <array>
#include <vector>

/**
 * A maqam that is compatible with (i.e. fully contained within) a
 * 12-pitch-class set.
 */
struct CompatibleMaqam
{
    juce::String maqamIdName;       // e.g. "maqam_rast_al-qarar_rast"
    juce::String maqamDisplayName;  // e.g. "Maqām Rāst [al-Qarār Rāst]"
    juce::String baseMaqamIdName;   // e.g. "maqam_rast" (untransposed)
    bool         isTransposed = false;
    juce::String tonicNoteName;     // Arabic note name of the tonic
    juce::String tonicIpnRef;       // IPN chromatic ref of the tonic, e.g. "C"
    juce::String version;
};

/**
 * A complete 12-pitch-class set as returned by the DiArMaqAr
 * classifyMaqamat12PitchClassSets endpoint.
 *
 * Each of the 12 slots corresponds to one chromatic IPN position
 * (C=0, C#=1, D=2, D#=3, E=4, F=5, F#=6, G=7, G#=8, A=9, A#=10, B=11).
 * The PitchClass stored in each slot is the specific pitch from the tuning
 * system that occupies that chromatic position for this maqam.
 *
 * Compatible maqamat are all maqamat (including transpositions) whose notes
 * map to exactly the same 12 chromatic positions with the same pitch classes.
 */
struct TwelvePitchClassSet
{
    juce::String sourceMaqamIdName;      // The maqam this set was derived from
    juce::String sourceMaqamDisplayName;

    // Slot i holds the pitch class for chromatic position i (0=C … 11=B).
    // A slot with !isValid() means that chromatic position is unoccupied
    // (uses the tuning system's default chromatic pitch for that slot).
    std::array<PitchClass, 12> slots;

    // All maqamat (including transpositions) that share this exact set
    std::vector<CompatibleMaqam> compatibleMaqamat;

    /**
     * Return the slider position (variant index) that this set implies for
     * each chromatic note, given the full variant list for each slot.
     * Returns -1 for a slot if no matching variant is found.
     */
    std::array<int, 12> resolveSliderPositions (
        const std::array<std::vector<PitchClass>, 12>& variantsPerSlot) const
    {
        std::array<int, 12> positions;
        positions.fill (-1);

        for (int i = 0; i < 12; ++i)
        {
            if (! slots[i].isValid()) continue;
            const auto& target = slots[i];
            const auto& variants = variantsPerSlot[i];

            for (int v = 0; v < (int) variants.size(); ++v)
            {
                // Match by note name (most reliable identifier)
                if (variants[v].noteName == target.noteName)
                {
                    positions[i] = v;
                    break;
                }
            }
        }
        return positions;
    }
};
