#include "NoteSliderComponent.h"

NoteSliderComponent::NoteSliderComponent()
{
    setOpaque (false);
}

// ── State setters ─────────────────────────────────────────────────────────────

void NoteSliderComponent::setSlotData (const ChromaticNoteVariants& slot,
                                       int ci, int midi, int effIdx)
{
    variants       = slot.variants;
    chromaticIndex = ci;
    midiNote       = midi;
    effectiveIndex = effIdx;
    slotCentsOffset = slot.centsOffset;
}

void NoteSliderComponent::setMaqamState (bool degree, bool degreeEquiv,
                                         bool tonic, bool tonicEquiv,
                                         bool modified, int maqamVarIdx)
{
    isMaqamDegree      = degree;
    isMaqamDegreeEquiv = degreeEquiv;
    isMaqamTonic       = tonic;
    isMaqamTonicEquiv  = tonicEquiv;
    isModified         = modified;
    maqamVariantIndex  = maqamVarIdx;
}

void NoteSliderComponent::setHeptState (bool muted, const juce::String& srcKey, bool whiteKey)
{
    isHeptMuted  = muted;
    heptSourceKey = srcKey;
    isWhiteKey   = whiteKey;
}

void NoteSliderComponent::setLabels (const juce::String& ipn, const juce::String& sol,
                                     const juce::String& pao)
{
    ipnLabel     = ipn;
    solfegeLabel = sol;
    paoNameLabel = pao;
}

void NoteSliderComponent::setOverrideState (bool hasVarOverride, bool hasPerNoteCts,
                                            double perNoteCtsVal)
{
    hasVariantOverride      = hasVarOverride;
    hasPerNoteCentsOverride = hasPerNoteCts;
    perNoteCentsValue       = perNoteCtsVal;
}

void NoteSliderComponent::setCentsOverride (double cents)
{
    hasCentsOverride  = true;
    centsOverrideValue = cents;
}

void NoteSliderComponent::clearCentsOverride()
{
    hasCentsOverride = false;
}

void NoteSliderComponent::setMidiHit (bool hit)
{
    if (isMidiHit != hit)
    {
        isMidiHit = hit;
        repaint();
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

double NoteSliderComponent::getEffectiveCents() const
{
    if (hasCentsOverride) return centsOverrideValue;
    if (hasPerNoteCentsOverride) return perNoteCentsValue;
    return slotCentsOffset;
}

juce::Rectangle<int> NoteSliderComponent::getTrackBounds() const
{
    // Track: centered horizontally, takes flex-1 of vertical space
    // Layout: 6px top padding, track (flex-1), then below track:
    //   4px gap, cents (14px), key bar (6px = 3px bar + 3px margin),
    //   ipn (14px), solfège (14px), pao name (28px = 2 lines), 4px bottom padding
    const int topPad = 6;
    const int bottomContent = 4 + 14 + 6 + 14 + 14 + 28 + 4;  // = 84px
    const int trackHeight = getHeight() - topPad - bottomContent;
    const int trackX = (getWidth() - Theme::kTrackWidth) / 2;
    return { trackX, topPad, Theme::kTrackWidth, std::max (40, trackHeight) };
}

double NoteSliderComponent::yToCents (int mouseY) const
{
    auto track = getTrackBounds();
    double pct = ((double)(mouseY - track.getY()) / (double) track.getHeight()) * 100.0;
    double cents = Theme::trackPctToCents (pct);
    return juce::jlimit (-150.0, 150.0, cents);
}

// ── Thumb and label colour resolution ─────────────────────────────────────────
// Priority order matches CSS specificity (last wins):
// 1. Normal (red)
// 2. Maqam degree equiv (red thumb, gold ring)
// 3. Maqam degree (gold)
// 4. Modified non-degree (teal)
// 5. Modified degree (cyan)
// 6. Per-note cents override (violet) — highest priority, overrides all above

juce::Colour NoteSliderComponent::getThumbColour() const
{
    // Detect modification: during drag, compare live cents to maqam variant default
    bool effectiveModified = isModified;
    if (isDragging && ! isPerNoteDrag && maqamVariantIndex >= 0
        && maqamVariantIndex < (int) variants.size())
    {
        if (std::abs (slotCentsOffset - variants[(size_t) maqamVariantIndex].midiCentsDeviation) > 0.01)
            effectiveModified = true;
    }

    if (isMidiHit)
    {
        if (isMaqamDegree || isMaqamDegreeEquiv || effectiveModified)
            return juce::Colours::white;
        return Theme::gold;
    }

    if (hasPerNoteCentsOverride || isPerNoteDrag)
        return Theme::violet;

    if (effectiveModified)
    {
        if (isMaqamDegree || isMaqamDegreeEquiv)
            return Theme::cyan;
        return Theme::teal;
    }

    if (isMaqamDegree)
        return Theme::gold;

    return Theme::sliderThumb;  // red
}

juce::Colour NoteSliderComponent::getThumbGlowColour() const
{
    bool effectiveModified = isModified;
    if (isDragging && ! isPerNoteDrag && maqamVariantIndex >= 0
        && maqamVariantIndex < (int) variants.size())
    {
        if (std::abs (slotCentsOffset - variants[(size_t) maqamVariantIndex].midiCentsDeviation) > 0.01)
            effectiveModified = true;
    }

    if (isMidiHit)
    {
        if (isMaqamDegree || isMaqamDegreeEquiv || effectiveModified)
            return juce::Colours::white.withAlpha (0.45f);
        return Theme::gold.withAlpha (0.45f);
    }

    if (hasPerNoteCentsOverride || isPerNoteDrag)
        return Theme::violet.withAlpha (0.5f);

    if (effectiveModified)
    {
        if (isMaqamDegree || isMaqamDegreeEquiv)
            return Theme::cyan.withAlpha (0.35f);
        return Theme::teal.withAlpha (0.35f);
    }

    if (hasVariantOverride)
        return Theme::blue.withAlpha (0.45f);

    if (isMaqamDegreeEquiv)
        return Theme::gold.withAlpha (0.5f);

    if (isMaqamDegree)
        return Theme::gold.withAlpha (0.35f);

    return Theme::sliderThumb.withAlpha (0.25f);  // red glow
}

juce::Colour NoteSliderComponent::getLabelColour() const
{
    if (hasPerNoteCentsOverride || isPerNoteDrag)
        return Theme::violet;

    if (hasVariantOverride)
        return Theme::accent;  // blue-ish via CSS --accent fallback

    if (isMaqamDegree || isMaqamDegreeEquiv)
        return Theme::gold;

    return Theme::textMuted;
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void NoteSliderComponent::paint (juce::Graphics& g)
{
    const float alpha = isHeptMuted ? 0.2f : 1.0f;

    // Tonic border
    if (isMaqamTonic)
    {
        g.setColour (Theme::gold.withAlpha (alpha));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    }
    else if (isMaqamTonicEquiv)
    {
        g.setColour (Theme::gold.withAlpha (0.3f * alpha));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    }

    auto trackBounds = getTrackBounds();
    const double cents = getEffectiveCents();
    const double thumbPct = Theme::centsToTrackPct (cents);
    const int thumbY = trackBounds.getY() + (int) (thumbPct / 100.0 * trackBounds.getHeight());

    // ── Snap markers (left of track) ──────────────────────────────────────
    {
        // Snap markers: positioned left of track with gap
        // CSS: .snap-markers { width: 8px; right: 100%; margin-right: 4px; }
        // marker center = 4px margin + 4px (half of 8px container) = 8px from track edge
        // Add extra 2px for visual breathing room in native rendering
        const int markerCentreX = trackBounds.getX() - 10;
        for (int i = 0; i < (int) variants.size(); ++i)
        {
            const double varPct = Theme::centsToTrackPct (variants[(size_t) i].midiCentsDeviation);
            const int markerY = trackBounds.getY() + (int) (varPct / 100.0 * trackBounds.getHeight());

            bool isActive = (i == effectiveIndex)
                            && (std::abs (cents - variants[(size_t) i].midiCentsDeviation) < 0.01);
            bool isMaqamDefault = (i == maqamVariantIndex);

            juce::Colour markerColour;
            if (isActive)
            {
                if (isModified)
                {
                    if (isMaqamDegree || isMaqamDegreeEquiv)
                        markerColour = Theme::cyan;
                    else
                        markerColour = Theme::teal;
                }
                else if (isMaqamDegree || isMaqamDegreeEquiv)
                    markerColour = Theme::gold;
                else
                    markerColour = Theme::sliderThumb;
            }
            else if (isMaqamDefault)
                markerColour = Theme::gold.withAlpha (0.6f);
            else
                markerColour = juce::Colour (0xff555580);

            g.setColour (markerColour.withAlpha (alpha));
            const int halfSize = Theme::kSnapMarkerSize / 2;
            g.fillEllipse ((float) (markerCentreX),
                           (float) (markerY - halfSize),
                           (float) Theme::kSnapMarkerSize,
                           (float) Theme::kSnapMarkerSize);
        }
    }

    // ── Track ─────────────────────────────────────────────────────────────
    g.setColour (Theme::sliderTrack.withAlpha (alpha));
    g.fillRoundedRectangle (trackBounds.toFloat(), 12.0f);

    // ── Thumb ─────────────────────────────────────────────────────────────
    {
        const float thumbR = (float) Theme::kThumbDiameter / 2.0f;
        const float thumbCX = (float) trackBounds.getCentreX();
        const float thumbCY = (float) thumbY;

        // Glow ring
        const float glowRadius = isDragging ? 6.0f : 3.0f;
        g.setColour (getThumbGlowColour().withMultipliedAlpha (alpha));
        g.fillEllipse (thumbCX - thumbR - glowRadius,
                       thumbCY - thumbR - glowRadius,
                       (thumbR + glowRadius) * 2.0f,
                       (thumbR + glowRadius) * 2.0f);

        // MIDI hit outer glow
        if (isMidiHit)
        {
            g.setColour (getThumbGlowColour().withAlpha (0.4f * alpha));
            g.fillEllipse (thumbCX - thumbR - 8.0f,
                           thumbCY - thumbR - 8.0f,
                           (thumbR + 8.0f) * 2.0f,
                           (thumbR + 8.0f) * 2.0f);
        }

        // Thumb circle
        g.setColour (getThumbColour().withMultipliedAlpha (alpha));
        g.fillEllipse (thumbCX - thumbR, thumbCY - thumbR,
                       thumbR * 2.0f, thumbR * 2.0f);
    }

    // ── Text below track ──────────────────────────────────────────────────
    int textY = trackBounds.getBottom() + 4;
    const int textW = getWidth();
    const auto labelColour = getLabelColour().withMultipliedAlpha (alpha);
    const auto mutedColour = Theme::textMuted.withMultipliedAlpha (alpha);

    // Cents value
    {
        g.setColour (mutedColour);
        g.setFont (Theme::scaledFont (10.0f));
        juce::String centsText = (cents >= 0.0 ? "+" : "")
                                 + juce::String (cents, 1) + juce::String::charToString (0x00A2); // ¢
        g.drawText (centsText, 0, textY, textW, 14, juce::Justification::centred);
        textY += 14;
    }

    // Piano key indicator bar
    {
        const int barW = (int) (getWidth() * 0.8f);
        const int barX = (getWidth() - barW) / 2;
        juce::Colour barColour;
        if (isHeptMuted)
            barColour = juce::Colours::transparentBlack;
        else if (isWhiteKey)
            barColour = Theme::whiteKey;
        else
            barColour = Theme::blackKey;

        g.setColour (barColour.withMultipliedAlpha (alpha));
        g.fillRoundedRectangle ((float) barX, (float) textY, (float) barW, 3.0f, 1.5f);
        textY += 6;
    }

    // IPN label — CSS: .ipn-label { font-size: 11px }
    {
        g.setColour (labelColour);
        g.setFont (Theme::scaledFont (11.0f));
        juce::String ipnText = ipnLabel;
        if (heptSourceKey.isNotEmpty())
            ipnText += juce::String::charToString (0x2192) + heptSourceKey;  // →
        g.drawText (ipnText, 0, textY, textW, 14, juce::Justification::centred);
        textY += 14;
    }

    // Solfège — CSS: .solfege { font-size: 11px; opacity: 0.8 }
    {
        g.setColour (labelColour.withAlpha (isMaqamDegree || isMaqamDegreeEquiv
                                            || hasPerNoteCentsOverride
                                            ? 1.0f : 0.8f));
        g.setFont (Theme::scaledFont (11.0f));
        g.drawText (solfegeLabel, 0, textY, textW, 14, juce::Justification::centred);
        textY += 14;
    }

    // PAO name (2-line) — CSS: word-break: keep-all, line-clamp: 2, line-height: 1.2
    // React splits on "/" and inserts <wbr/> after "/" for line wrapping
    // Never squish text — use two separate drawText calls for two lines
    {
        juce::Colour nameColour = (isMaqamDegree || isMaqamDegreeEquiv)
                                  ? Theme::gold : Theme::text;
        g.setColour (nameColour.withMultipliedAlpha (alpha));
        g.setFont (Theme::scaledFont (11.0f));

        const int lineH = 14;
        if (paoNameLabel.contains ("/"))
        {
            // Split at "/" — first part on line 1, second on line 2
            int slashIdx = paoNameLabel.indexOf ("/");
            juce::String line1 = paoNameLabel.substring (0, slashIdx + 1).trim();
            juce::String line2 = paoNameLabel.substring (slashIdx + 1).trim();
            g.drawText (line1, 2, textY, textW - 4, lineH, juce::Justification::centredTop);
            g.drawText (line2, 2, textY + lineH, textW - 4, lineH, juce::Justification::centredTop);
        }
        else
        {
            g.drawText (paoNameLabel, 2, textY, textW - 4, 28, juce::Justification::centredTop);
        }
    }
}

// ── Mouse handling ────────────────────────────────────────────────────────────

juce::MouseCursor NoteSliderComponent::getMouseCursor()
{
    // During drag: closed/grabbing hand
    if (isDragging)
        return juce::MouseCursor::DraggingHandCursor;

    if (isHeptMuted || variants.empty())
        return juce::MouseCursor::NormalCursor;

    // Use last known mouse position for hit testing
    auto mousePos = getMouseXYRelative();
    auto trackBounds = getTrackBounds();

    // Check if mouse is over the thumb circle
    const double cents = getEffectiveCents();
    const double thumbPct = Theme::centsToTrackPct (cents);
    const int thumbY = trackBounds.getY() + (int) (thumbPct / 100.0 * trackBounds.getHeight());
    const int thumbCX = trackBounds.getCentreX();
    const int thumbR = Theme::kThumbDiameter / 2;
    const int dx = mousePos.x - thumbCX;
    const int dy = mousePos.y - thumbY;
    if (dx * dx + dy * dy <= (thumbR + 3) * (thumbR + 3))
        return juce::MouseCursor (juce::MouseCursor::DraggingHandCursor);

    // Check if mouse is over a specific snap marker dot
    const int markerCX = trackBounds.getX() - 10;
    const int halfSize = Theme::kSnapMarkerSize / 2 + 2;
    for (int i = 0; i < (int) variants.size(); ++i)
    {
        const double varPct = Theme::centsToTrackPct (variants[(size_t) i].midiCentsDeviation);
        const int markerY = trackBounds.getY() + (int) (varPct / 100.0 * trackBounds.getHeight());
        if (std::abs (mousePos.x - markerCX) <= halfSize && std::abs (mousePos.y - markerY) <= halfSize)
            return juce::MouseCursor::PointingHandCursor;
    }

    return juce::MouseCursor::NormalCursor;
}

void NoteSliderComponent::mouseDown (const juce::MouseEvent& e)
{
    if (isHeptMuted || variants.empty()) return;

    // Check if click is on a snap marker (10px gap from track edge)
    auto trackBounds = getTrackBounds();
    const int markerLeft = trackBounds.getX() - 10 - Theme::kSnapMarkerSize / 2;
    const int markerRight = trackBounds.getX() - 10 + Theme::kSnapMarkerSize / 2;

    if (e.x >= markerLeft && e.x <= markerRight + 4)
    {
        // Find closest snap marker
        int bestIdx = -1;
        int bestDist = 999;
        for (int i = 0; i < (int) variants.size(); ++i)
        {
            const double varPct = Theme::centsToTrackPct (variants[(size_t) i].midiCentsDeviation);
            const int markerY = trackBounds.getY() + (int) (varPct / 100.0 * trackBounds.getHeight());
            const int dist = std::abs (e.y - markerY);
            if (dist < bestDist && dist < 10)
            {
                bestDist = dist;
                bestIdx = i;
            }
        }
        if (bestIdx >= 0 && onVariantSelect)
        {
            onVariantSelect (chromaticIndex, bestIdx);
            return;
        }
    }

    // Begin drag
    isDragging = true;
    isPerNoteDrag = e.mods.isShiftDown();

    if (! isPerNoteDrag && onGestureStart)
        onGestureStart (chromaticIndex);

    double cents = yToCents (e.y);
    if (isPerNoteDrag)
    {
        perNoteCentsValue = cents;
        hasPerNoteCentsOverride = true;
        if (onNoteCentsDrag) onNoteCentsDrag (midiNote, cents);
    }
    else
    {
        slotCentsOffset = cents;
        if (onCentsDrag) onCentsDrag (chromaticIndex, cents);
    }

    repaint();
}

void NoteSliderComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! isDragging) return;

    double cents = yToCents (e.y);
    if (isPerNoteDrag)
    {
        // Update local state for immediate visual feedback during shift+drag
        perNoteCentsValue = cents;
        hasPerNoteCentsOverride = true;
        if (onNoteCentsDrag) onNoteCentsDrag (midiNote, cents);
    }
    else
    {
        // Update local cents for immediate cyan thumb feedback during drag
        slotCentsOffset = cents;
        if (onCentsDrag) onCentsDrag (chromaticIndex, cents);
    }
    repaint();
}

void NoteSliderComponent::mouseUp (const juce::MouseEvent& e)
{
    if (! isDragging) return;

    isDragging = false;
    double cents = yToCents (e.y);

    if (isPerNoteDrag)
    {
        if (onNoteCentsDragEnd) onNoteCentsDragEnd (midiNote, cents);
    }
    else
    {
        if (onCentsDragEnd) onCentsDragEnd (chromaticIndex, cents);
        if (onGestureEnd) onGestureEnd (chromaticIndex);
    }

    isPerNoteDrag = false;
    repaint();
}
