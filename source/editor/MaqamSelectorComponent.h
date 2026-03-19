#pragma once
#include "SearchablePopup.h"
#include "TanghimTheme.h"
#include "../model/TuningSystem.h"
#include "../model/MaqamListEntry.h"
#include <functional>
#include <map>

/**
 * Native JUCE replacement for MaqamSelector.tsx + TuningSystemSelector.tsx.
 *
 * Two sections:
 * 1. Tuning System: system dropdown + starting note dropdown (red accent)
 * 2. Maqam: maqam dropdown + transposition dropdown (gold accent)
 */
class TuningSystemSelectorComponent : public juce::Component
{
public:
    TuningSystemSelectorComponent();

    void setSystems (const std::vector<TuningSystem>& systems);
    void setCurrentSelection (const juce::String& systemId, const juce::String& startingNote);
    void setCacheStatus (const std::map<juce::String, bool>& cacheMap);
    void setSystemPlaceholder (const juce::String& text);
    void setNotePlaceholder (const juce::String& text);
    void setNoteEnabled (bool enabled);

    std::function<void (const juce::String& systemId, const juce::String& startingNote)> onSelect;

    void resized() override;

private:
    SearchableSelect systemSelect;
    SearchableSelect noteSelect;

    std::vector<TuningSystem> tuningSystems;
    juce::String currentSystemId;
    juce::String currentStartingNote;
    std::map<juce::String, bool> cacheStatus;

    void updateNoteItems();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuningSystemSelectorComponent)
};

/**
 * Native JUCE replacement for MaqamSelector.tsx — maqam + transposition dropdowns.
 */
class MaqamSelectorComponent : public juce::Component
{
public:
    MaqamSelectorComponent();

    void setMaqamList (const std::vector<MaqamListEntry>& list);
    void setSelectedMaqam (const juce::String& maqamId, int transpositionIndex);
    void setModified (bool isModified);
    void setActivePresetIndex (int idx);
    /** Set the PAO tonic IDs that are in octaves 1–2 (for transposition filtering),
     *  and a MIDI note order map for sorting transpositions by pitch. */
    void setTranspositionFilter (const std::set<juce::String>& allowed,
                                  const std::map<juce::String, int>& midiOrder);
    /** Set cache status for the current system+startingNote.
     *  Shows ✓ (cached) or ↓ (not cached) on all maqam/transposition items. */
    void setCacheStatus (bool hasMaqamListCached);
    void setMaqamPlaceholder (const juce::String& text);
    void setTranspositionPlaceholder (const juce::String& text);
    void setMaqamEnabled (bool enabled);
    void setTranspositionEnabled (bool enabled);

    std::function<void (const juce::String& maqamId, int transpositionIndex)> onSelect;

    void resized() override;

private:
    SearchableSelect maqamSelect;
    SearchableSelect transpositionSelect;

    std::vector<MaqamListEntry> maqamList;
    juce::String selectedMaqamId;
    int selectedTransIdx = -1;
    bool isModified = false;
    int activePresetIdx = -1;
    std::set<juce::String> allowedTonicIds;  // tonic IDs in octaves 1–2
    std::map<juce::String, int> paoMidiOrder; // PAO name → MIDI note for pitch sorting
    bool maqamListCached = false;

    void updateTranspositionItems();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MaqamSelectorComponent)
};
