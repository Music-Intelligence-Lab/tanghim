#pragma once
#include "TanghimTheme.h"
#include <functional>
#include <vector>

/**
 * Custom searchable dropdown popup — replaces CustomSelect.tsx.
 * JUCE's ComboBox doesn't support live search/filter, so this is a custom Component
 * with a TextEditor search field and a ListBox for results.
 */
class SearchablePopup : public juce::Component,
                        private juce::ListBoxModel
{
public:
    struct Item
    {
        juce::String value;
        juce::String label;
        juce::String suffix;
        bool suffixCached = false;  // true = green (✓ cached), false = grey (↓ uncached)
    };

    SearchablePopup();

    void setItems (const std::vector<Item>& items);
    void setSelectedValue (const juce::String& value);
    void setAccentColour (juce::Colour colour);  // red for tuning, gold for maqam

    std::function<void (const juce::String& value)> onChange;
    std::function<void()> onDismissed;  // Called when popup is dismissed via outside click

    // Show as a popup attached to a component
    void showAt (juce::Component* trigger);
    void dismiss();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void inputAttemptWhenModal() override;
    bool keyPressed (const juce::KeyPress& key) override;

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool isSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent& e) override;

private:
    std::vector<Item> allItems;
    std::vector<int>  filteredIndices;  // indices into allItems
    juce::String      selectedValue;
    int               highlightedRow = -1;
    juce::Colour      accentColour = Theme::accent;

    juce::TextEditor  searchField;
    juce::ListBox     listBox { {}, this };

    void filterItems();
    static juce::String stripDiacritics (const juce::String& text);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchablePopup)
};

/**
 * Trigger button + popup wrapper — the full CustomSelect replacement.
 * Shows a button that opens a SearchablePopup when clicked.
 */
class SearchableSelect : public juce::Component
{
public:
    SearchableSelect();

    void setItems (const std::vector<SearchablePopup::Item>& items);
    void setSelectedValue (const juce::String& value);
    void setPlaceholder (const juce::String& text);
    void setSearchable (bool searchable);
    void setAccentColour (juce::Colour colour);
    void setSelectedSuffix (const juce::String& suffix);
    void setGoldFill (bool enabled);

    std::function<void (const juce::String& value)> onChange;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    std::vector<SearchablePopup::Item> items;
    juce::String selectedValue;
    juce::String selectedLabel;
    juce::String placeholder = "Select...";
    juce::String selectedSuffix;
    juce::Colour accentColour = Theme::accent;
    bool searchable = true;
    bool isOpen = false;
    bool goldFill = false;

    std::unique_ptr<SearchablePopup> activePopup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchableSelect)
};
