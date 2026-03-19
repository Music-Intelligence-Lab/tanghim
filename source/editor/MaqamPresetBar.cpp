#include "MaqamPresetBar.h"

static constexpr const char* NOTE_NAMES[12] = {
    "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
};

MaqamPresetBar::MaqamPresetBar()
{
    midiPresetNotes.fill (-1);
    compatible.fill (true);
}

void MaqamPresetBar::setPresets (const std::array<MaqamPreset, 16>& p)
{
    presets = p;
    repaint();
}

void MaqamPresetBar::setActivePresetIndex (int idx)
{
    activePresetIndex = idx;
    repaint();
}

void MaqamPresetBar::setMidiLearnTarget (int idx)
{
    midiLearnTarget = idx;
    repaint();
}

void MaqamPresetBar::setMidiPresetNotes (const std::array<int, 16>& notes)
{
    midiPresetNotes = notes;
    repaint();
}

void MaqamPresetBar::setPresetCompatible (const std::array<bool, 8>& comp)
{
    compatible = comp;
    repaint();
}

juce::String MaqamPresetBar::midiToNoteName (int note)
{
    if (note < 0 || note > 127) return {};
    int ci = note % 12;
    int octave = note / 12 - 1;
    return juce::String (NOTE_NAMES[ci]) + juce::String (octave);
}

void MaqamPresetBar::resized()
{
    auto area = getLocalBounds();
    const int cols = 4;
    const int rows = 2;
    const int colGap = 6;  // CSS: gap: 4px 6px (row col)
    const int rowGap = 4;
    const int btnW = (area.getWidth() - colGap * (cols - 1)) / cols;
    const int btnH = (area.getHeight() - rowGap * (rows - 1)) / rows;

    for (int i = 0; i < 8; ++i)
    {
        int col = i % cols;
        int row = i / cols;
        int x = area.getX() + col * (btnW + colGap);
        int y = area.getY() + row * (btnH + rowGap);

        buttons[(size_t) i].bounds = { x, y, btnW, btnH };
        // Clear button (top-right corner, 14x14)
        buttons[(size_t) i].clearBounds = { x + btnW - 16, y + 2, 14, 14 };
        // MIDI badge (right side, 30x14)
        buttons[(size_t) i].midiBadgeBounds = { x + btnW - 32, y + btnH - 16, 30, 14 };
    }
}

void MaqamPresetBar::paint (juce::Graphics& g)
{
    for (int i = 0; i < 8; ++i)
        drawPresetButton (g, i);
}

void MaqamPresetBar::drawPresetButton (juce::Graphics& g, int index)
{
    const auto& preset = presets[(size_t) index];
    const auto& btn = buttons[(size_t) index];
    auto bounds = btn.bounds.toFloat();

    bool isActive = (activePresetIndex == index);
    bool isLearning = (midiLearnTarget == index);
    bool isDisabled = ! compatible[(size_t) index];
    bool isAssigned = preset.isAssigned;
    float alpha = isDisabled ? 0.4f : 1.0f;

    // Background — CSS: .preset-btn { background: surface2, border: 1px solid border }
    //                   .preset-btn.assigned { border-color: accent2 (#0f3460) }
    //                   .preset-btn.active { border-color: #d4a843, background: rgba(212,168,67,0.1) }
    if (isActive)
    {
        g.setColour (Theme::goldMaqam.withAlpha (0.1f * alpha));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (Theme::goldMaqam.withAlpha (alpha));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
    }
    else
    {
        g.setColour (Theme::surface2.withAlpha (alpha));
        g.fillRoundedRectangle (bounds, 4.0f);
        // Assigned presets use accent2 border, unassigned use default border
        g.setColour ((isAssigned ? Theme::accent2 : Theme::border).withAlpha (alpha));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
    }

    // Preset number — CSS: position absolute, top 2px, left 4px, font-size 9px
    g.setColour ((isActive ? Theme::goldMaqam : Theme::textMuted).withAlpha (alpha));
    g.setFont (Theme::scaledFont (9.0f));
    g.drawText (juce::String (index + 1),
                juce::Rectangle<int> (btn.bounds.getX() + 4, btn.bounds.getY() + 2, 14, 12),
                juce::Justification::centredLeft);

    if (isAssigned)
    {
        // Maqam name + tonic treated as one vertically-centered block
        juce::String label = preset.maqamDisplayName;
        label = label.replace ("al-", juce::String ("al") + juce::String::charToString (0x2011));

        juce::String tonicLine;
        if (preset.tonicNoteName.isNotEmpty())
            tonicLine += preset.tonicNoteName;
        if (preset.tonicIpnRef.isNotEmpty())
            tonicLine += (tonicLine.isEmpty() ? "" : " | ") + preset.tonicIpnRef;
        if (preset.tonicSolfege.isNotEmpty())
            tonicLine += (tonicLine.isEmpty() ? "" : " | ") + preset.tonicSolfege;

        // Measure combined block height: name (14px) + tonic (12px) + gap (2px) = 28px
        const int nameH = 14;
        const int tonicH = 12;
        const int blockGap = 2;
        const int totalH = nameH + (tonicLine.isNotEmpty() ? blockGap + tonicH : 0);

        // Center block vertically in button
        auto contentArea = btn.bounds.reduced (6, 0);
        const int blockY = contentArea.getY() + (contentArea.getHeight() - totalH) / 2;

        // Maqam name — font-size 12px
        if (isActive)
        {
            g.setColour (Theme::goldMaqam.withAlpha (alpha));
            g.setFont (Theme::scaledFont (12.0f));
        }
        else
        {
            g.setColour (Theme::accent.withAlpha (alpha));
            g.setFont (Theme::scaledFont (12.0f));
        }
        g.drawText (label, contentArea.withY (blockY).withHeight (nameH),
                    juce::Justification::centred, true);

        // Tonic line — font-size 10px, color text-muted
        if (tonicLine.isNotEmpty())
        {
            g.setColour (Theme::textMuted.withAlpha (alpha));
            g.setFont (Theme::scaledFont (10.0f));
            g.drawText (tonicLine, contentArea.withY (blockY + nameH + blockGap).withHeight (tonicH),
                        juce::Justification::centred, true);
        }

        // Clear button (×) — highlight on hover
        {
            bool clearHovered = (hoveredClearButton == index);
            if (clearHovered)
            {
                g.setColour (juce::Colour (0xffef5350).withAlpha (alpha));  // Red on hover
                g.fillRoundedRectangle (btn.clearBounds.toFloat(), 3.0f);
                g.setColour (juce::Colours::white.withAlpha (alpha));
            }
            else
            {
                g.setColour (Theme::textMuted.withAlpha (alpha * 0.6f));
            }
            g.setFont (Theme::scaledFont (12.0f));
            g.drawText (juce::String::charToString (0x00D7),
                        btn.clearBounds, juce::Justification::centred);
        }
    }

    // MIDI badge
    int midiNote = midiPresetNotes[(size_t) index];
    if (isLearning)
    {
        g.setColour (Theme::accent.withAlpha (0.3f));
        g.fillRoundedRectangle (btn.midiBadgeBounds.toFloat(), 3.0f);
        g.setColour (Theme::accent);
        g.setFont (Theme::scaledFont (9.0f));
        g.drawText ("...", btn.midiBadgeBounds, juce::Justification::centred);
    }
    else if (midiNote >= 0)
    {
        g.setColour (Theme::accent.withAlpha (0.2f));
        g.fillRoundedRectangle (btn.midiBadgeBounds.toFloat(), 3.0f);
        g.setColour (Theme::accent.withAlpha (alpha));
        g.setFont (Theme::scaledFont (9.0f));
        g.drawText (midiToNoteName (midiNote), btn.midiBadgeBounds, juce::Justification::centred);
    }
}

void MaqamPresetBar::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 8; ++i)
    {
        if (! buttons[(size_t) i].bounds.contains (e.getPosition())) continue;
        if (! compatible[(size_t) i]) return;

        const auto& preset = presets[(size_t) i];

        // MIDI badge click area
        if (buttons[(size_t) i].midiBadgeBounds.contains (e.getPosition()))
        {
            if (e.mods.isShiftDown())
            {
                // Re-learn
                if (onMidiLearnStart) onMidiLearnStart (i);
            }
            else if (midiPresetNotes[(size_t) i] >= 0)
            {
                // Clear mapping
                if (onMidiNoteClear) onMidiNoteClear (i);
            }
            return;
        }

        // Clear button click
        if (preset.isAssigned && buttons[(size_t) i].clearBounds.contains (e.getPosition()))
        {
            if (onClearPreset) onClearPreset (i);
            return;
        }

        // Shift+click = MIDI Learn
        if (e.mods.isShiftDown())
        {
            if (midiLearnTarget == i)
            {
                if (onMidiLearnCancel) onMidiLearnCancel();
            }
            else
            {
                if (onMidiLearnStart) onMidiLearnStart (i);
            }
            return;
        }

        // Normal click
        if (activePresetIndex == i)
        {
            // Deactivate
            if (onDeactivatePreset) onDeactivatePreset (i);
        }
        else if (preset.isAssigned)
        {
            // Apply
            if (onApplyPreset) onApplyPreset (i);
        }
        else
        {
            // Save to empty slot
            if (onSavePreset) onSavePreset (i);
        }
        return;
    }
}

void MaqamPresetBar::mouseMove (const juce::MouseEvent& e)
{
    // Track × button hover
    int newHover = -1;
    for (int i = 0; i < 8; ++i)
    {
        if (presets[(size_t) i].isAssigned
            && buttons[(size_t) i].clearBounds.contains (e.getPosition()))
        {
            newHover = i;
            break;
        }
    }

    if (newHover != hoveredClearButton)
    {
        hoveredClearButton = newHover;
        repaint();
    }

    // Tooltips
    juce::String tooltip;
    for (int i = 0; i < 8; ++i)
    {
        if (buttons[(size_t) i].bounds.contains (e.getPosition()))
        {
            if (presets[(size_t) i].isAssigned)
                tooltip = "Click to activate | Shift+Click to MIDI map";
            else
                tooltip = "Click to save preset | Shift+Click to MIDI map";
            break;
        }
    }

    if (tooltip != lastTooltip)
    {
        lastTooltip = tooltip;
        setTooltip (tooltip);
    }
}

void MaqamPresetBar::mouseExit (const juce::MouseEvent&)
{
    if (hoveredClearButton >= 0)
    {
        hoveredClearButton = -1;
        repaint();
    }
    if (lastTooltip.isNotEmpty())
    {
        lastTooltip = {};
        setTooltip ({});
    }
}
