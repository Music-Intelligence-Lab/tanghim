#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <vector>

/**
 * One of the 12 user-configurable maqam preset buttons.
 *
 * When assigned, stores the maqam identity and the exact slider positions
 * it implies, so pressing the button instantly snaps all 12 sliders.
 */
struct MaqamPreset
{
    bool isAssigned = false;

    // Maqam identity
    juce::String maqamIdName;           // e.g. "maqam_rast_al-qarar_rast"
    juce::String maqamDisplayName;      // e.g. "Maqām Rāst"
    juce::String baseMaqamIdName;       // e.g. "maqam_rast"
    bool         isTransposed   = false;
    juce::String tonicNoteName;
    juce::String tonicIpnRef;           // e.g. "C"
    int          pitchClassSetIndex = -1; // Index into the loaded TwelvePitchClassSet list

    // The 12 slider variant indices this maqam implies (index into
    // ChromaticNoteVariants::variants for each slot 0-11).
    // -1 means "don't change this slot" (note not in the maqam).
    std::array<int, 12> sliderPositions = { 0,0,0,0,0,0,0,0,0,0,0,0 };

    // Ascending degree PAO names (e.g. ["rast","dugah","segah",...]).
    // Used to check compatibility when switching tuning systems.
    std::vector<juce::String> degreeNames;

    void clear()
    {
        isAssigned      = false;
        maqamIdName     = {};
        maqamDisplayName = {};
        baseMaqamIdName = {};
        isTransposed    = false;
        tonicNoteName   = {};
        tonicIpnRef     = {};
        pitchClassSetIndex = -1;
        sliderPositions.fill (0);
        degreeNames.clear();
    }

    juce::String buttonLabel() const
    {
        if (! isAssigned) return {};
        return maqamDisplayName.isEmpty() ? maqamIdName : maqamDisplayName;
    }
};
