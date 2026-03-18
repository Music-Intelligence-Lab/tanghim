#include "ReferenceFreqControl.h"
#include <cmath>

static constexpr const char* IPN_REFS[12] = {
    "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
};

ReferenceFreqControl::ReferenceFreqControl()
{
    ipnNames.fill ({});

    // Hz input
    hzInput.setFont (juce::FontOptions (12.0f));
    hzInput.setJustification (juce::Justification::centred);
    hzInput.setColour (juce::TextEditor::backgroundColourId, Theme::surface2);
    hzInput.setColour (juce::TextEditor::outlineColourId,    Theme::border);
    hzInput.setColour (juce::TextEditor::textColourId,       Theme::text);
    hzInput.setColour (juce::TextEditor::focusedOutlineColourId, Theme::accent);
    hzInput.setInputRestrictions (7, "0123456789.");
    hzInput.onReturnKey = [this] { onHzInputReturn(); };
    hzInput.onEscapeKey = [this] { hzInput.setText (juce::String (currentHz, 2)); };
    addAndMakeVisible (hzInput);

    // Semitone buttons
    auto setupBtn = [this] (juce::TextButton& btn, double deltaCents)
    {
        btn.setColour (juce::TextButton::buttonColourId,  Theme::surface2);
        btn.setColour (juce::TextButton::textColourOffId,  Theme::textMuted);
        btn.onClick = [this, deltaCents]
        {
            double newCents = juce::jlimit (kMinCents, kMaxCents, currentCents + deltaCents);
            if (onGestureStart) onGestureStart();
            if (onDragEnd) onDragEnd (newCents);
            if (onGestureEnd) onGestureEnd();
        };
        addAndMakeVisible (btn);
    };
    setupBtn (minusBtn, -100.0);
    setupBtn (plusBtn,   100.0);
}

// ── State setters ─────────────────────────────────────────────────────────────

void ReferenceFreqControl::setCents (double cents)
{
    currentCents = cents;
    repaint();
}

void ReferenceFreqControl::setHzValues (double currHz, double defHz)
{
    currentHz = currHz;
    defaultHz = defHz;
    // Update Hz display unless user is actively editing
    if (! hzInput.hasKeyboardFocus (false))
        hzInput.setText (juce::String (currentHz, 2) + " Hz", juce::dontSendNotification);
    repaint();
}

void ReferenceFreqControl::setTonicInfo (int tonicCI,
                                         const std::array<juce::String, 12>& names)
{
    tonicChromatic = tonicCI;
    ipnNames = names;
    repaint();
}

// ── Hz input ──────────────────────────────────────────────────────────────────

void ReferenceFreqControl::onHzInputReturn()
{
    double newHz = hzInput.getText().getDoubleValue();
    if (newHz <= 0.0 || defaultHz <= 0.0) return;

    // Convert Hz to cents offset: cents = 1200 * log2(newHz / defaultHz)
    double cents = 1200.0 * std::log2 (newHz / defaultHz);
    cents = juce::jlimit (kMinCents, kMaxCents, cents);

    if (onGestureStart) onGestureStart();
    if (onDragEnd) onDragEnd (cents);
    if (onGestureEnd) onGestureEnd();

    unfocusAllComponents();
}

// ── Layout ────────────────────────────────────────────────────────────────────

void ReferenceFreqControl::resized()
{
    // CSS: padding 4px 10px, gap 8px
    // Layout: [Ref Freq label] [knob] [hz input] [− btn] [cents column] [+ btn]
    auto area = getLocalBounds().reduced (10, 4);
    const int gap = 8;

    // 1. "Ref Freq" label — painted, not a component (skip space for it)
    area.removeFromLeft (42 + gap);

    // 2. Arc knob — painted, skip space
    area.removeFromLeft (kKnobSize + gap);

    // 3. Hz input (72px)
    hzInput.setBounds (area.removeFromLeft (72).withSizeKeepingCentre (72, 20));
    area.removeFromLeft (gap);

    // 4. − button (20×20)
    minusBtn.setBounds (area.removeFromLeft (20).withSizeKeepingCentre (20, 20));

    // 5. + button (20×20) from right
    plusBtn.setBounds (area.removeFromRight (20).withSizeKeepingCentre (20, 20));

    // 6. Cents column — remaining space between − and + buttons (painted, skip)
}

juce::Rectangle<int> ReferenceFreqControl::getKnobBounds() const
{
    // After "Ref Freq" label (42px) + gap (8px) = starts at x=60
    const int labelW = 42;
    const int gap = 8;
    const int x = 10 + labelW + gap;  // 10px padding + label + gap
    return { x, (getHeight() - kKnobSize) / 2, kKnobSize, kKnobSize };
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void ReferenceFreqControl::paint (juce::Graphics& g)
{
    // CSS: background surface2, border 1px solid border, border-radius 4px
    auto bgBounds = getLocalBounds().toFloat();
    g.setColour (Theme::surface2);
    g.fillRoundedRectangle (bgBounds, 4.0f);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (bgBounds.reduced (0.5f), 4.0f, 1.0f);

    auto area = getLocalBounds().reduced (10, 4);
    const int gap = 8;

    // 1. "Ref Freq" label — CSS: font-size 10px, color text-muted
    {
        auto labelArea = area.removeFromLeft (42);
        g.setColour (Theme::textMuted);
        g.setFont (Theme::scaledFont (10.0f));
        g.drawText ("Ref Freq", labelArea, juce::Justification::centredLeft);
        area.removeFromLeft (gap);
    }

    // 2. Arc knob
    auto knobBounds = juce::Rectangle<float> ((float) area.getX(), (float) area.getY(),
                                               (float) kKnobSize, (float) area.getHeight());
    drawArcKnob (g, knobBounds.withSizeKeepingCentre ((float) kKnobSize, (float) kKnobSize));
    area.removeFromLeft (kKnobSize + gap);

    // 3. Hz input — handled by JUCE TextEditor component, skip
    area.removeFromLeft (72 + gap);

    // 4. − button — skip (JUCE component)
    area.removeFromLeft (20);

    // 6. + button — skip from right (JUCE component)
    area.removeFromRight (20);

    // 5. Cents column — remaining space between − and + buttons, centered
    {
        auto centsCol = area;

        // Top: cents value — CSS: font-size 11px, color text-muted
        g.setColour (Theme::textMuted);
        g.setFont (Theme::scaledFont (11.0f));
        juce::String centsText = (currentCents >= 0.0 ? "+" : "")
                                 + juce::String (currentCents, 2) + " "
                                 + juce::String::charToString (0x00A2); // ¢
        g.drawText (centsText, centsCol.removeFromTop (centsCol.getHeight() / 2),
                    juce::Justification::centred);

        // Bottom: transposition label — CSS: font-size 10px, color accent
        juce::String transLabel = getTranspositionLabel();
        if (transLabel.isNotEmpty())
        {
            g.setColour (Theme::accent);
            g.setFont (Theme::scaledFont (10.0f));
            g.drawText (transLabel, centsCol, juce::Justification::centred);
        }
    }
}

void ReferenceFreqControl::drawArcKnob (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float r = kKnobRadius;

    // Background arc — React: -135° to +135° with 0° at 12 o'clock
    // JUCE addCentredArc: 0 radians = 12 o'clock (top), clockwise positive
    // -135° = -0.75π rad, +135° = +0.75π rad
    const float startAngle = -juce::MathConstants<float>::pi * 0.75f;
    const float endAngle   =  juce::MathConstants<float>::pi * 0.75f;

    juce::Path bgArc;
    bgArc.addCentredArc (cx, cy, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::textMuted.withAlpha (0.35f));
    g.strokePath (bgArc, juce::PathStrokeType (2.0f));

    // Value arc — 0 cents = 0° (12 o'clock), ±700 cents = ±135°
    const float normValue = (float) (currentCents / kMaxCents);  // -1 to +1
    const float valueAngle = normValue * juce::MathConstants<float>::pi * 0.75f;
    const float zeroAngle = 0.0f;  // 12 o'clock

    if (std::abs (currentCents) > 0.5)
    {
        juce::Path valArc;
        valArc.addCentredArc (cx, cy, r, r, 0.0f,
                              std::min (zeroAngle, valueAngle),
                              std::max (zeroAngle, valueAngle), true);
        g.setColour (Theme::accent);
        g.strokePath (valArc, juce::PathStrokeType (2.5f));
    }

    // Center dot
    g.setColour (Theme::textMuted.withAlpha (0.5f));
    g.fillEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);

    // Indicator dot on arc — JUCE: 0 rad = top, so use sin/cos with offset
    // angle 0 = top (12 o'clock): x = sin(angle), y = -cos(angle)
    const float dotX = cx + r * std::sin (valueAngle);
    const float dotY = cy - r * std::cos (valueAngle);
    g.setColour (Theme::accent);
    g.fillEllipse (dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);
}

// ── Transposition label ───────────────────────────────────────────────────────

juce::String ReferenceFreqControl::getTranspositionLabel() const
{
    if (tonicChromatic < 0) return {};
    if (std::abs (currentCents) < 0.5) return {};

    int semitones = (int) std::round (currentCents / 100.0);
    double remainder = currentCents - semitones * 100.0;

    int fromIdx = tonicChromatic;
    int toIdx = ((tonicChromatic + semitones) % 12 + 12) % 12;

    juce::String fromName = ipnNames[(size_t) fromIdx].isNotEmpty()
                            ? ipnNames[(size_t) fromIdx]
                            : juce::String (IPN_REFS[fromIdx]);
    juce::String toName = ipnNames[(size_t) toIdx].isNotEmpty()
                          ? ipnNames[(size_t) toIdx]
                          : juce::String (IPN_REFS[toIdx]);

    // Arrow direction
    juce::String arrow;
    if (semitones > 0)
        arrow = juce::String::charToString (0x2197);  // ↗
    else if (semitones < 0)
        arrow = juce::String::charToString (0x2198);  // ↘
    else
        arrow = juce::String::charToString (0x2192);  // →

    juce::String suffix;
    if (std::abs (remainder) > 0.5)
        suffix = (remainder > 0 ? "+" : juce::String::charToString (0x2212));

    return fromName + " " + arrow + " " + toName + suffix;
}

// ── Mouse handling (knob drag) ────────────────────────────────────────────────

void ReferenceFreqControl::mouseDown (const juce::MouseEvent& e)
{
    if (! getKnobBounds().contains (e.getPosition())) return;

    // Unfocus the Hz input so it doesn't block updates during drag
    hzInput.unfocusAllComponents();

    isDragging = true;
    dragStartCents = currentCents;
    dragStartY = e.y;

    if (onGestureStart) onGestureStart();
}

void ReferenceFreqControl::mouseDrag (const juce::MouseEvent& e)
{
    if (! isDragging) return;

    const double sensitivity = e.mods.isShiftDown() ? 0.3 : 2.0;
    const double deltaCents = (dragStartY - e.y) * sensitivity;
    double newCents = juce::jlimit (kMinCents, kMaxCents, dragStartCents + deltaCents);

    if (onDrag) onDrag (newCents);
}

void ReferenceFreqControl::mouseUp (const juce::MouseEvent& e)
{
    if (! isDragging) return;

    isDragging = false;
    const double sensitivity = e.mods.isShiftDown() ? 0.3 : 2.0;
    const double deltaCents = (dragStartY - e.y) * sensitivity;
    double newCents = juce::jlimit (kMinCents, kMaxCents, dragStartCents + deltaCents);

    if (onDragEnd) onDragEnd (newCents);
    if (onGestureEnd) onGestureEnd();
}

void ReferenceFreqControl::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! getKnobBounds().contains (e.getPosition())) return;

    if (onGestureStart) onGestureStart();
    if (onDragEnd) onDragEnd (0.0);
    if (onGestureEnd) onGestureEnd();
}
