#include "OutputBadges.h"

OutputBadges::OutputBadges()
{
    rebuildBadges();
}

void OutputBadges::setOscillatorEnabled (bool enabled)
{
    oscEnabled = enabled;
    rebuildBadges();
    repaint();
}

void OutputBadges::setHeptEnabled (bool enabled)
{
    heptEnabled = enabled;
    rebuildBadges();
    repaint();
}

void OutputBadges::setHasMaqam (bool has)
{
    hasMaqam = has;
    rebuildBadges();
    repaint();
}

void OutputBadges::setMtsStatus (bool transmitter, int native, int mpe, int mono)
{
    isMtsTransmitter = transmitter;
    mtsNativeCount   = native;
    mpeCount         = mpe;
    monoPbCount      = mono;
    rebuildBadges();
    repaint();
}

void OutputBadges::rebuildBadges()
{
    badges.clear();

    // Row 1: toggle badges (always shown)
    badges.push_back ({ "Osc",  Theme::oscAmber,   oscEnabled,  true, -1, {} });
    badges.push_back ({ "Hept", Theme::heptMagenta, heptEnabled && hasMaqam, true, -1, {} });

    // Row 2: status badges (always shown, greyed out when inactive)
    badges.push_back ({ "MTS-ESP", Theme::mtsEsp,    isMtsTransmitter, false, mtsNativeCount, {} });
    badges.push_back ({ "MPE",     Theme::mpeBadge,   mpeCount > 0,     false, mpeCount, {} });
    badges.push_back ({ "Mono PB", Theme::monoPbBadge, monoPbCount > 0, false, monoPbCount, {} });

    // Recalculate bounds if we have a size
    if (getWidth() > 0)
        resized();
}

void OutputBadges::resized()
{
    if (badges.empty()) return;

    // CSS: container padding 4px 10px, rows gap 4px, badges gap 6px
    // CSS: .mts-row > .mts-badge { flex: 1; justify-content: center; }
    // Each badge stretches equally within its row
    const int rowGap = 4;
    const int badgeGap = 6;
    auto area = getLocalBounds().reduced (10, 4);
    const int badgeH = (area.getHeight() - rowGap) / 2;

    // Row 1: Osc, Hept — flex: 1 each (equal width)
    {
        const int row1Count = 2;
        const int totalGap = badgeGap * (row1Count - 1);
        const int badgeW = (area.getWidth() - totalGap) / row1Count;
        int x = area.getX();
        int y = area.getY();
        for (int i = 0; i < row1Count && i < (int) badges.size(); ++i)
        {
            badges[(size_t) i].bounds = { x, y, badgeW, badgeH };
            x += badgeW + badgeGap;
        }
    }

    // Row 2: MTS-ESP, MPE, Mono PB — flex: 1 each (equal width)
    {
        const int row2Start = 2;
        const int row2Count = (int) badges.size() - row2Start;
        if (row2Count > 0)
        {
            const int totalGap = badgeGap * (row2Count - 1);
            const int badgeW = (area.getWidth() - totalGap) / row2Count;
            int x = area.getX();
            int y = area.getY() + badgeH + rowGap;
            for (int i = row2Start; i < (int) badges.size(); ++i)
            {
                badges[(size_t) i].bounds = { x, y, badgeW, badgeH };
                x += badgeW + badgeGap;
            }
        }
    }
}

void OutputBadges::paint (juce::Graphics& g)
{
    // Container background
    g.setColour (Theme::surface2);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 1.0f);

    for (const auto& badge : badges)
        drawBadge (g, badge);
}

void OutputBadges::drawBadge (juce::Graphics& g, const BadgeInfo& badge)
{
    auto bounds = badge.bounds.toFloat();
    float alpha = badge.active ? 1.0f : 0.5f;

    // CSS default: background var(--surface), border 1px solid var(--border)
    // Toggle active: colored fill at 0.2 opacity, colored border at 0.7
    // Toggle inactive: default bg/border, opacity 0.5
    // Status badges: default bg, colored border at 0.4

    if (badge.toggleable && badge.active)
    {
        // Active toggle: colored fill + colored border
        g.setColour (badge.colour.withAlpha (0.2f));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (badge.colour.withAlpha (0.7f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
    }
    else
    {
        // Default or inactive: surface background + border
        g.setColour (Theme::surface);
        g.fillRoundedRectangle (bounds, 3.0f);

        if (badge.toggleable)
        {
            // Inactive toggle: colored border at 0.7, but whole badge at 0.5 opacity
            g.setColour (badge.colour.withAlpha (0.7f * alpha));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
        }
        else
        {
            // Status badge: colored border at 0.4
            g.setColour (badge.colour.withAlpha (badge.active ? 0.4f : 0.2f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
        }
    }

    // Label text — CSS: font-size 11px, padding 2px 8px
    g.setColour (badge.colour.withAlpha (alpha));
    g.setFont (Theme::scaledFont (11.0f));

    if (badge.count >= 0)
    {
        g.drawText (badge.label, bounds.reduced (8.0f, 0.0f).toNearestInt(),
                    juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (juce::String (badge.count),
                    bounds.reduced (8.0f, 0.0f).toNearestInt(),
                    juce::Justification::centredRight);
    }
    else
    {
        g.drawText (badge.label, bounds.toNearestInt(), juce::Justification::centred);
    }
}

void OutputBadges::mouseDown (const juce::MouseEvent& e)
{
    for (const auto& badge : badges)
    {
        if (badge.bounds.contains (e.getPosition()) && badge.toggleable)
        {
            if (badge.label == "Osc" && onOscToggle)
                onOscToggle (! oscEnabled);
            else if (badge.label == "Hept" && hasMaqam && onHeptToggle)
                onHeptToggle (! heptEnabled);
            return;
        }
    }
}
