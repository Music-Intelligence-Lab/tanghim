#include "MaqamSelectorComponent.h"

// ══════════════════════════════════════════════════════════════════════════════
// TuningSystemSelectorComponent
// ══════════════════════════════════════════════════════════════════════════════

TuningSystemSelectorComponent::TuningSystemSelectorComponent()
{
    systemSelect.setPlaceholder ("Select Tuning System");
    systemSelect.setAccentColour (Theme::accent);
    systemSelect.onChange = [this] (const juce::String& value)
    {
        currentSystemId = value;
        // Reset to first starting note
        for (const auto& sys : tuningSystems)
        {
            if (sys.id == value && sys.startingNoteIds.size() > 0)
            {
                currentStartingNote = sys.startingNoteIds[0];
                updateNoteItems();
                noteSelect.setSelectedValue (currentStartingNote);
                break;
            }
        }
        if (onSelect) onSelect (currentSystemId, currentStartingNote);
    };
    addAndMakeVisible (systemSelect);

    noteSelect.setPlaceholder ("Starting Note Name");
    noteSelect.setAccentColour (Theme::accent);
    noteSelect.onChange = [this] (const juce::String& value)
    {
        currentStartingNote = value;
        if (onSelect) onSelect (currentSystemId, currentStartingNote);
    };
    addAndMakeVisible (noteSelect);
}

void TuningSystemSelectorComponent::setSystems (const std::vector<TuningSystem>& systems)
{
    tuningSystems = systems;

    std::vector<SearchablePopup::Item> items;
    for (const auto& sys : tuningSystems)
    {
        // Check if ANY starting note for this system is cached
        // Cache keys are "systemId:startingNote"
        bool anyCached = false;
        for (const auto& noteId : sys.startingNoteIds)
        {
            auto it = cacheStatus.find (sys.id + ":" + noteId);
            if (it != cacheStatus.end() && it->second)
            { anyCached = true; break; }
        }
        juce::String suffix;
        if (! cacheStatus.empty())
            suffix = anyCached ? juce::String::charToString (0x2713) : juce::String::charToString (0x2193);

        items.push_back ({ sys.id, sys.displayName, suffix, anyCached });
    }
    systemSelect.setItems (items);
    updateNoteItems();
}

void TuningSystemSelectorComponent::setCurrentSelection (const juce::String& sysId,
                                                         const juce::String& startNote)
{
    currentSystemId = sysId;
    currentStartingNote = startNote;
    systemSelect.setSelectedValue (sysId);
    updateNoteItems();
    noteSelect.setSelectedValue (startNote);
}

void TuningSystemSelectorComponent::setCacheStatus (const std::map<juce::String, bool>& cache)
{
    cacheStatus = cache;
    // Re-set systems to update suffixes
    setSystems (tuningSystems);
}

void TuningSystemSelectorComponent::setSystemPlaceholder (const juce::String& text) { systemSelect.setPlaceholder (text); }
void TuningSystemSelectorComponent::setNotePlaceholder (const juce::String& text)   { noteSelect.setPlaceholder (text); }
void TuningSystemSelectorComponent::setNoteEnabled (bool enabled)                   { noteSelect.setEnabled (enabled); }

void TuningSystemSelectorComponent::updateNoteItems()
{
    for (const auto& sys : tuningSystems)
    {
        if (sys.id == currentSystemId)
        {
            std::vector<SearchablePopup::Item> items;
            for (int i = 0; i < sys.startingNoteIds.size(); ++i)
            {
                juce::String suffix;
                auto key = sys.id + ":" + sys.startingNoteIds[i];
                auto it = cacheStatus.find (key);
                if (it != cacheStatus.end())
                    suffix = it->second ? juce::String::charToString (0x2713)
                                        : juce::String::charToString (0x2193);

                bool cached = it != cacheStatus.end() && it->second;
                items.push_back ({ sys.startingNoteIds[i],
                                   sys.startingNoteDisplayNames[i], suffix, cached });
            }
            noteSelect.setItems (items);
            break;
        }
    }
}

void TuningSystemSelectorComponent::resized()
{
    // CSS: flex-direction column, gap 4px
    auto area = getLocalBounds();
    const int gap = 4;
    const int rowH = (area.getHeight() - gap) / 2;
    systemSelect.setBounds (area.removeFromTop (rowH));
    area.removeFromTop (gap);
    noteSelect.setBounds (area);
}

// ══════════════════════════════════════════════════════════════════════════════
// MaqamSelectorComponent
// ══════════════════════════════════════════════════════════════════════════════

MaqamSelectorComponent::MaqamSelectorComponent()
{
    maqamSelect.setPlaceholder (juce::CharPointer_UTF8 ("Maq\xc4\x81m"));
    maqamSelect.setSearchable (true);
    maqamSelect.setAccentColour (Theme::goldMaqam);
    maqamSelect.onChange = [this] (const juce::String& value)
    {
        selectedMaqamId = value;
        selectedTransIdx = -1;
        updateTranspositionItems();
        transpositionSelect.setSelectedValue ({});
        if (onSelect) onSelect (selectedMaqamId, selectedTransIdx);
    };
    addAndMakeVisible (maqamSelect);

    transpositionSelect.setPlaceholder ("Tonic");
    transpositionSelect.setSearchable (true);
    transpositionSelect.setAccentColour (Theme::goldMaqam);
    transpositionSelect.onChange = [this] (const juce::String& value)
    {
        selectedTransIdx = value.getIntValue();
        if (onSelect) onSelect (selectedMaqamId, selectedTransIdx);
    };
    addAndMakeVisible (transpositionSelect);
}

void MaqamSelectorComponent::setCacheStatus (bool hasMaqamList)
{
    maqamListCached = hasMaqamList;
    // Re-set maqam list to update suffixes
    setMaqamList (maqamList);
}

void MaqamSelectorComponent::setMaqamPlaceholder (const juce::String& text)        { maqamSelect.setPlaceholder (text); }
void MaqamSelectorComponent::setTranspositionPlaceholder (const juce::String& text) { transpositionSelect.setPlaceholder (text); }
void MaqamSelectorComponent::setMaqamEnabled (bool enabled)                         { maqamSelect.setEnabled (enabled); }
void MaqamSelectorComponent::setTranspositionEnabled (bool enabled)                 { transpositionSelect.setEnabled (enabled); }

void MaqamSelectorComponent::setMaqamList (const std::vector<MaqamListEntry>& list)
{
    maqamList = list;

    // All maqamat come from the same system+startingNote cache entry.
    // Show ✓ if the maqam list is cached, ↓ if not (list will be empty when uncached,
    // but we still set the flag so the indicator updates when data arrives).
    juce::String suffix = maqamListCached ? juce::String::charToString (0x2713)
                                          : juce::String::charToString (0x2193);

    std::vector<SearchablePopup::Item> items;
    for (const auto& entry : maqamList)
        items.push_back ({ entry.maqamId, entry.maqamDisplay, suffix, maqamListCached });

    std::sort (items.begin(), items.end(),
               [] (const SearchablePopup::Item& a, const SearchablePopup::Item& b)
               { return a.label.compareIgnoreCase (b.label) < 0; });

    maqamSelect.setItems (items);
    updateTranspositionItems();
}

void MaqamSelectorComponent::setSelectedMaqam (const juce::String& maqamId, int transIdx)
{
    selectedMaqamId = maqamId;
    selectedTransIdx = transIdx;

    maqamSelect.setSelectedValue (maqamId);
    if (isModified)
        maqamSelect.setSelectedSuffix (" *");
    else
        maqamSelect.setSelectedSuffix ({});

    updateTranspositionItems();
    transpositionSelect.setSelectedValue (juce::String (transIdx));
}

void MaqamSelectorComponent::setModified (bool mod)
{
    isModified = mod;
    maqamSelect.setSelectedSuffix (isModified ? " *" : "");
}

void MaqamSelectorComponent::setActivePresetIndex (int idx)
{
    activePresetIdx = idx;
    // CSS: .gold-fill applied when maqam selected via dropdown (not via preset)
    bool gold = selectedMaqamId.isNotEmpty() && activePresetIdx < 0;
    maqamSelect.setGoldFill (gold);
    transpositionSelect.setGoldFill (gold);
}

void MaqamSelectorComponent::setTranspositionFilter (const std::set<juce::String>& allowed,
                                                       const std::map<juce::String, int>& midiOrder)
{
    allowedTonicIds = allowed;
    paoMidiOrder = midiOrder;
    updateTranspositionItems();
}

void MaqamSelectorComponent::updateTranspositionItems()
{
    for (const auto& entry : maqamList)
    {
        if (entry.maqamId == selectedMaqamId)
        {
            // Collect all options: base + filtered transpositions
            struct TransOption { juce::String value; juce::String label; int midiOrder; };
            std::vector<TransOption> options;

            // Base (qarār)
            {
                auto orderIt = paoMidiOrder.find (entry.tonicId);
                int order = orderIt != paoMidiOrder.end() ? orderIt->second : 0;
                options.push_back ({ "-1",
                    entry.tonicDisplay + juce::String (juce::CharPointer_UTF8 (" (qar\xc4\x81r)")),
                    order });
            }

            // Transpositions — filter to octaves 1–2 via allowedTonicIds
            for (int i = 0; i < (int) entry.transpositions.size(); ++i)
            {
                const auto& trans = entry.transpositions[(size_t) i];
                if (! allowedTonicIds.empty() && allowedTonicIds.count (trans.tonicId) == 0)
                    continue;
                auto orderIt = paoMidiOrder.find (trans.tonicId);
                int order = orderIt != paoMidiOrder.end() ? orderIt->second : 999;
                options.push_back ({ juce::String (i), trans.tonicDisplay, order });
            }

            // Sort by ascending MIDI pitch order (base qarār in its natural position)
            std::sort (options.begin(), options.end(),
                       [] (const TransOption& a, const TransOption& b) { return a.midiOrder < b.midiOrder; });

            juce::String suffix = maqamListCached ? juce::String::charToString (0x2713)
                                                    : juce::String::charToString (0x2193);

            std::vector<SearchablePopup::Item> items;
            for (const auto& opt : options)
                items.push_back ({ opt.value, opt.label, suffix, maqamListCached });

            transpositionSelect.setItems (items);
            return;
        }
    }
    transpositionSelect.setItems ({});
}

void MaqamSelectorComponent::resized()
{
    auto area = getLocalBounds();
    const int gap = 8;
    const int transW = std::max (180, area.getWidth() / 2);
    transpositionSelect.setBounds (area.removeFromRight (transW));
    area.removeFromRight (gap);
    maqamSelect.setBounds (area);
}
