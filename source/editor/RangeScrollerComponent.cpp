#include "RangeScrollerComponent.h"
#include <cmath>

RangeScrollerComponent::RangeScrollerComponent()
{
}

void RangeScrollerComponent::setStartMidi (double midi)
{
    startMidi = midi;
    repaint();
}

void RangeScrollerComponent::setVisibleCount (int count)
{
    visibleCount = std::max (1, count);
    repaint();
}

void RangeScrollerComponent::setMaqamTonicMidi (int midi)
{
    maqamTonicMidi = midi;
}

// ── Coordinate conversion ─────────────────────────────────────────────────────

juce::Rectangle<int> RangeScrollerComponent::getTrackArea() const
{
    auto area = getLocalBounds().reduced (16, 0);
    area.removeFromLeft (44);   // "Range" label (40px) + gap (4px)
    area.removeFromRight (60);  // IPN range text (~56px) + gap (4px)
    return area;
}

double RangeScrollerComponent::xToMidi (int x) const
{
    auto area = getTrackArea();
    double frac = (double) (x - area.getX()) / (double) area.getWidth();
    return frac * (128.0 - visibleCount);
}

int RangeScrollerComponent::midiToX (double midi) const
{
    auto area = getTrackArea();
    double frac = midi / (128.0 - visibleCount);
    return area.getX() + (int) (frac * area.getWidth());
}

double RangeScrollerComponent::applyMagneticSnap (double midi) const
{
    // Snap to C, G, A positions if within tolerance (pixel-based)
    // Convert 1.1 MIDI notes to pixels and only snap if cursor is within that many pixels
    auto area = getLocalBounds().reduced (16, 0);
    const double midiPerPixel = (128.0 - visibleCount) / std::max (1, area.getWidth());
    const double snapToleranceMidi = std::min (SNAP_TOLERANCE, midiPerPixel * 8.0);  // max 8px snap radius

    for (int note = 0; note < 128; ++note)
    {
        int ci = note % 12;
        if (ci == 0 || ci == 7 || ci == 9)  // C, G, A
        {
            if (std::abs (midi - note) < snapToleranceMidi)
                return (double) note;
        }
    }
    return midi;
}

juce::String RangeScrollerComponent::midiToIpn (int midi) const
{
    if (midi < 0 || midi > 127) return {};
    int ci = midi % 12;
    int octave = midi / 12 - 1;
    return juce::String (IPN_NAMES[ci]) + juce::String (octave);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void RangeScrollerComponent::paint (juce::Graphics& g)
{
    auto fullArea = getLocalBounds().reduced (16, 4);

    // "Range" label — CSS: font-size 11px, font-weight 600
    g.setColour (Theme::textMuted);
    g.setFont (Theme::scaledFont (11.0f, juce::Font::bold));
    g.drawText ("Range", fullArea.removeFromLeft (40), juce::Justification::centredLeft);

    // Track area (constrained between "Range" and IPN labels)
    auto area = getTrackArea().withY (fullArea.getY()).withHeight (fullArea.getHeight());

    // Track background
    const int trackH = 4;
    auto trackRect = area.withHeight (trackH).withY (area.getCentreY() - trackH / 2);
    g.setColour (Theme::sliderTrack);
    g.fillRoundedRectangle (trackRect.toFloat(), 2.0f);

    // Tick marks (C, G, A positions)
    for (int note = 0; note < 128; ++note)
    {
        int ci = note % 12;
        if (ci != 0 && ci != 7 && ci != 9) continue;

        int x = midiToX ((double) note);
        if (x < area.getX() || x > area.getRight()) continue;

        g.setColour (juce::Colour (0xff444468));
        g.fillRoundedRectangle ((float) x - 1.0f, (float) (area.getCentreY() - 5),
                                2.0f, 10.0f, 1.0f);
    }

    // Thumb — CSS: 12×12 circle, border-radius: 50%
    const int thumbX = midiToX (startMidi);
    g.setColour (Theme::sliderThumb);
    g.fillEllipse ((float) thumbX - 6.0f, (float) (area.getCentreY() - 6),
                   12.0f, 12.0f);

    // Range label — CSS: font-size 11px, font-weight 600, tabular-nums
    int displayStart = (int) std::round (startMidi);
    int displayEnd   = std::min (127, displayStart + visibleCount);
    juce::String rangeText = midiToIpn (displayStart) + juce::String::charToString (0x2013)
                             + midiToIpn (displayEnd);
    g.setColour (Theme::text);
    g.setFont (Theme::scaledFont (11.0f, juce::Font::bold));
    // Right edge aligned with preset bar (16px padding from right edge)
    auto labelArea = getLocalBounds().reduced (16, 0);
    g.drawText (rangeText, labelArea.removeFromRight (64),
                juce::Justification::centredRight);
}

// ── Mouse handling ────────────────────────────────────────────────────────────

void RangeScrollerComponent::mouseDown (const juce::MouseEvent& e)
{
    isDragging = true;
    double midi = applyMagneticSnap (xToMidi (e.x));
    midi = juce::jlimit (0.0, 128.0 - visibleCount, midi);
    startMidi = midi;
    if (onChange) onChange (startMidi);
    repaint();
}

void RangeScrollerComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! isDragging) return;
    double midi = applyMagneticSnap (xToMidi (e.x));
    midi = juce::jlimit (0.0, 128.0 - visibleCount, midi);
    startMidi = midi;
    if (onChange) onChange (startMidi);
    repaint();
}

void RangeScrollerComponent::mouseUp (const juce::MouseEvent&)
{
    isDragging = false;
}

void RangeScrollerComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    if (maqamTonicMidi < 0) return;

    // Center maqam octave (13 notes)
    int padding = std::max (0, (visibleCount - 13) / 2);
    double centered = (double) (maqamTonicMidi - padding);
    centered = juce::jlimit (0.0, 128.0 - visibleCount, centered);
    startMidi = centered;
    if (onChange) onChange (startMidi);
    repaint();
}
