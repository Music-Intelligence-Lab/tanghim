#pragma once
#include "TanghimTheme.h"
#include <functional>

/**
 * Native JUCE replacement for RangeScroller.tsx — horizontal slider
 * for scroll position with magnetic snap to C/G/A tick marks.
 */
class RangeScrollerComponent : public juce::Component
{
public:
    RangeScrollerComponent();

    void setStartMidi (double midi);
    void setVisibleCount (int count);
    void setMaqamTonicMidi (int midi);

    std::function<void (double startMidi)> onChange;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    double startMidi     = 48.0;
    int    visibleCount  = 12;
    int    maqamTonicMidi = -1;

    bool   isDragging    = false;

    // Tick marks at C, G, A positions
    static constexpr int TICK_CLASSES[3] = { 0, 7, 9 };  // C, G, A
    static constexpr double SNAP_TOLERANCE = 1.1;

    juce::Rectangle<int> getTrackArea() const;
    double xToMidi (int x) const;
    int    midiToX (double midi) const;
    double applyMagneticSnap (double midi) const;
    juce::String midiToIpn (int midi) const;

    static constexpr const char* IPN_NAMES[12] = {
        "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RangeScrollerComponent)
};
