#include "SearchablePopup.h"

// ── Diacritics stripping (replaces JS NFD normalize + regex) ──────────────────
// Basic Latin diacritics decomposition for search matching.
// Covers common Arabic transliteration characters (ā, ī, ū, etc.)

juce::String SearchablePopup::stripDiacritics (const juce::String& text)
{
    juce::String result;
    for (int i = 0; i < text.length(); ++i)
    {
        auto ch = text[i];
        // Skip combining diacritical marks (U+0300–U+036F)
        if (ch >= 0x0300 && ch <= 0x036F) continue;
        // Common transliteration mappings
        switch (ch)
        {
            case 0x0101: case 0x00E0: case 0x00E1: case 0x00E2: result += 'a'; break;  // ā à á â
            case 0x012B: case 0x00EC: case 0x00ED: case 0x00EE: result += 'i'; break;  // ī ì í î
            case 0x016B: case 0x00F9: case 0x00FA: case 0x00FB: result += 'u'; break;  // ū ù ú û
            case 0x0113: case 0x00E8: case 0x00E9: case 0x00EA: result += 'e'; break;  // ē è é ê
            case 0x014D: case 0x00F2: case 0x00F3: case 0x00F4: result += 'o'; break;  // ō ò ó ô
            case 0x1E63: result += 's'; break;  // ṣ
            case 0x1E0D: result += 'd'; break;  // ḍ
            case 0x1E6D: result += 't'; break;  // ṭ
            case 0x1E25: result += 'h'; break;  // ḥ
            case 0x1E93: result += 'z'; break;  // ẓ
            case 0x02BB: case 0x02BC: case 0x2018: case 0x2019: break;  // ʻ ʼ ' ' — skip
            default: result += juce::CharacterFunctions::toLowerCase (ch); break;
        }
    }
    return result;
}

// ══════════════════════════════════════════════════════════════════════════════
// SearchablePopup
// ══════════════════════════════════════════════════════════════════════════════

SearchablePopup::SearchablePopup()
{
    setWantsKeyboardFocus (true);

    searchField.setFont (Theme::scaledFont (12.0f));
    searchField.setColour (juce::TextEditor::backgroundColourId,    Theme::surface);
    searchField.setColour (juce::TextEditor::outlineColourId,       Theme::border);
    searchField.setColour (juce::TextEditor::textColourId,          Theme::text);
    searchField.setColour (juce::TextEditor::focusedOutlineColourId, Theme::accent);
    searchField.onTextChange = [this] { filterItems(); };
    searchField.setEscapeAndReturnKeysConsumed (false);
    searchField.addKeyListener (this);  // Intercept up/down/return/escape before TextEditor
    addAndMakeVisible (searchField);

    listBox.setColour (juce::ListBox::backgroundColourId, Theme::bg);
    listBox.setRowHeight (28);
    addAndMakeVisible (listBox);
}

void SearchablePopup::setItems (const std::vector<Item>& newItems)
{
    allItems = newItems;
    filterItems();
}

void SearchablePopup::setSelectedValue (const juce::String& value)
{
    selectedValue = value;
    listBox.repaint();
}

void SearchablePopup::setAccentColour (juce::Colour colour)
{
    accentColour = colour;
}

void SearchablePopup::showAt (juce::Component* trigger)
{
    if (trigger == nullptr) return;

    searchField.clear();
    filterItems();

    // Find the top-level editor component to add popup as child
    auto* topLevel = trigger->getTopLevelComponent();
    if (topLevel == nullptr) return;

    // Calculate bounds relative to the top-level component
    auto triggerBounds = trigger->getLocalBounds();
    auto topLeftInParent = trigger->localPointToGlobal (juce::Point<int> (0, triggerBounds.getHeight() + 2));
    auto topLeftInTopLevel = topLevel->getLocalPoint (nullptr, topLeftInParent);

    int popupW = std::max (trigger->getWidth(), 200);
    int popupH = std::min (400, (int) allItems.size() * 28 + 40);

    // Clamp within the top-level bounds
    auto tlBounds = topLevel->getLocalBounds();
    if (topLeftInTopLevel.y + popupH > tlBounds.getBottom() - 30)
        popupH = tlBounds.getBottom() - 30 - topLeftInTopLevel.y;
    popupH = std::max (60, popupH);

    setBounds (topLeftInTopLevel.x, topLeftInTopLevel.y, popupW, popupH);
    topLevel->addAndMakeVisible (this);
    toFront (true);
    searchField.grabKeyboardFocus();

    // Scroll to selected item
    for (int i = 0; i < (int) filteredIndices.size(); ++i)
    {
        if (allItems[(size_t) filteredIndices[(size_t) i]].value == selectedValue)
        {
            listBox.scrollToEnsureRowIsOnscreen (i);
            break;
        }
    }

    // Start watching for outside clicks to dismiss
    enterModalState (false);
}

void SearchablePopup::inputAttemptWhenModal()
{
    // Called when user clicks outside the modal popup — dismiss it
    dismiss();
    if (onDismissed) onDismissed();
}

void SearchablePopup::dismiss()
{
    if (isCurrentlyModal())
        exitModalState (0);
    setVisible (false);
    if (auto* parent = getParentComponent())
        parent->removeChildComponent (this);
}

void SearchablePopup::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
}

void SearchablePopup::resized()
{
    auto area = getLocalBounds().reduced (4);
    searchField.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);
    listBox.setBounds (area);
}

void SearchablePopup::filterItems()
{
    filteredIndices.clear();
    juce::String query = stripDiacritics (searchField.getText().trim());

    for (int i = 0; i < (int) allItems.size(); ++i)
    {
        if (query.isEmpty() || stripDiacritics (allItems[(size_t) i].label).contains (query))
            filteredIndices.push_back (i);
    }

    listBox.updateContent();
    listBox.repaint();
}

int SearchablePopup::getNumRows()
{
    return (int) filteredIndices.size();
}

void SearchablePopup::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool /*isSelected*/)
{
    if (row < 0 || row >= (int) filteredIndices.size()) return;

    const auto& item = allItems[(size_t) filteredIndices[(size_t) row]];
    bool isActive = (item.value == selectedValue);
    bool isHighlighted = (row == highlightedRow);

    if (isHighlighted)
    {
        g.setColour (accentColour.withAlpha (0.15f));
        g.fillRect (0, 0, w, h);
    }
    else if (isActive)
    {
        g.setColour (accentColour.withAlpha (0.1f));
        g.fillRect (0, 0, w, h);
    }

    g.setFont (Theme::scaledFont (12.0f));

    if (item.suffix.isNotEmpty())
    {
        // Draw suffix right-aligned first (flex-shrink: 0, always visible)
        const int suffixW = 20;
        g.setColour (item.suffixCached ? juce::Colour (0xff66bb6a).withAlpha (0.8f)
                                        : Theme::textMuted.withAlpha (0.4f));
        g.setFont (Theme::scaledFont (11.0f));
        g.drawText (item.suffix, w - suffixW - 8, 0, suffixW, h, juce::Justification::centredRight);

        // Draw label with ellipsis truncation (flex: 1)
        g.setColour (isActive ? accentColour : Theme::text);
        g.setFont (Theme::scaledFont (12.0f));
        g.drawText (item.label, 8, 0, w - suffixW - 20, h,
                    juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (isActive ? accentColour : Theme::text);
        g.drawText (item.label, 8, 0, w - 16, h, juce::Justification::centredLeft);
    }
}

// KeyListener override — intercepts keys from searchField before TextEditor consumes them
bool SearchablePopup::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return handleKey (key);
}

// Component override — handles keys when popup itself has focus
bool SearchablePopup::keyPressed (const juce::KeyPress& key)
{
    return handleKey (key);
}

bool SearchablePopup::handleKey (const juce::KeyPress& key)
{
    const int numRows = (int) filteredIndices.size();
    if (numRows == 0) return false;

    if (key == juce::KeyPress::upKey)
    {
        highlightedRow = (highlightedRow <= 0) ? numRows - 1 : highlightedRow - 1;
        listBox.scrollToEnsureRowIsOnscreen (highlightedRow);
        listBox.repaint();
        return true;
    }
    if (key == juce::KeyPress::downKey)
    {
        highlightedRow = (highlightedRow >= numRows - 1) ? 0 : highlightedRow + 1;
        listBox.scrollToEnsureRowIsOnscreen (highlightedRow);
        listBox.repaint();
        return true;
    }
    if (key == juce::KeyPress::returnKey)
    {
        if (highlightedRow >= 0 && highlightedRow < numRows)
        {
            const juce::String value = allItems[(size_t) filteredIndices[(size_t) highlightedRow]].value;
            selectedValue = value;
            dismiss();
            if (onChange) onChange (value);
        }
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        dismiss();
        if (onDismissed) onDismissed();
        return true;
    }
    return false;
}

void SearchablePopup::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int) filteredIndices.size()) return;

    // Copy value before calling onChange — the callback may delete this popup
    const juce::String value = allItems[(size_t) filteredIndices[(size_t) row]].value;
    selectedValue = value;

    dismiss();  // Remove from parent BEFORE onChange (which may reset the owning unique_ptr)

    if (onChange) onChange (value);
}

// ══════════════════════════════════════════════════════════════════════════════
// SearchableSelect (trigger button + popup)
// ══════════════════════════════════════════════════════════════════════════════

SearchableSelect::SearchableSelect()
{
    setInterceptsMouseClicks (true, false);
}

void SearchableSelect::setItems (const std::vector<SearchablePopup::Item>& newItems)
{
    items = newItems;
    // Update label if value matches
    for (const auto& item : items)
    {
        if (item.value == selectedValue)
        {
            selectedLabel = item.label;
            repaint();
            return;
        }
    }
}

void SearchableSelect::setSelectedValue (const juce::String& value)
{
    selectedValue = value;
    selectedLabel = {};
    for (const auto& item : items)
    {
        if (item.value == value)
        {
            selectedLabel = item.label;
            break;
        }
    }
    repaint();
}

void SearchableSelect::setPlaceholder (const juce::String& text)
{
    placeholder = text;
    repaint();
}

void SearchableSelect::setSearchable (bool s)
{
    searchable = s;
}

void SearchableSelect::setAccentColour (juce::Colour colour)
{
    accentColour = colour;
}

void SearchableSelect::setSelectedSuffix (const juce::String& suffix)
{
    selectedSuffix = suffix;
    repaint();
}

void SearchableSelect::setGoldFill (bool enabled)
{
    if (goldFill != enabled)
    {
        goldFill = enabled;
        repaint();
    }
}

void SearchableSelect::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background — CSS: .gold-fill { background: rgba(212,168,67,0.1); border-color: #d4a843 }
    if (goldFill)
    {
        g.setColour (Theme::goldMaqam.withAlpha (0.1f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (Theme::goldMaqam);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    }
    else
    {
        g.setColour (Theme::surface2);
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (isOpen ? accentColour : Theme::border);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    }

    // Label text
    juce::String displayText = selectedLabel.isNotEmpty()
                               ? selectedLabel + selectedSuffix
                               : placeholder;
    auto textColour = goldFill ? Theme::goldMaqam
                               : (selectedLabel.isNotEmpty() ? juce::Colour (0xffb8b8c8) : Theme::textMuted);
    if (! isEnabled())
        textColour = textColour.withAlpha (0.35f);
    g.setColour (textColour);

    g.setFont (Theme::scaledFont (12.0f));  // CSS: font-size 12px
    g.drawText (displayText, bounds.reduced (10.0f, 0.0f).toNearestInt(),
                juce::Justification::centredLeft);

    // Chevron
    const float chevX = bounds.getRight() - 16.0f;
    const float chevY = bounds.getCentreY();
    juce::Path chevron;
    chevron.addTriangle (chevX - 4.0f, chevY - 2.0f,
                         chevX + 4.0f, chevY - 2.0f,
                         chevX, chevY + 3.0f);
    g.setColour (Theme::textMuted);
    g.fillPath (chevron);
}

void SearchableSelect::resized()
{
}

void SearchableSelect::mouseDown (const juce::MouseEvent&)
{
    if (! isEnabled()) return;
    if (items.empty()) return;

    // Dismiss existing popup if open
    if (activePopup)
    {
        activePopup->dismiss();
        activePopup.reset();
        isOpen = false;
        repaint();
        return;
    }

    activePopup = std::make_unique<SearchablePopup>();
    activePopup->setItems (items);
    activePopup->setSelectedValue (selectedValue);
    activePopup->setAccentColour (accentColour);

    activePopup->onChange = [this] (const juce::String& value)
    {
        selectedValue = value;
        for (const auto& item : items)
        {
            if (item.value == value)
            {
                selectedLabel = item.label;
                break;
            }
        }
        isOpen = false;
        repaint();
        if (onChange) onChange (value);
        juce::MessageManager::callAsync ([this] { activePopup.reset(); });
    };

    // Handle outside-click dismiss (popup calls inputAttemptWhenModal → dismiss)
    activePopup->onDismissed = [this]
    {
        isOpen = false;
        repaint();
        juce::MessageManager::callAsync ([this] { activePopup.reset(); });
    };

    isOpen = true;
    repaint();

    activePopup->showAt (this);
}
