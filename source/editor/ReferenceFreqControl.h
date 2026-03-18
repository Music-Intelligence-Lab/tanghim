#pragma once
#include "TanghimTheme.h"
#include <functional>

/**
 * Native JUCE replacement for ReferenceFreqControl.tsx — SVG arc knob
 * for reference frequency offset (±700 cents).
 *
 * Layout: [Arc knob] [Hz input] [−100 btn] [+100 btn] | [cents display] | [transposition label]
 */
class ReferenceFreqControl : public juce::Component
{
public:
    ReferenceFreqControl();

    // ── State setters ────────────────────────────────────────────────────
    void setCents (double cents);
    void setHzValues (double currentHz, double defaultHz);
    void setTonicInfo (int tonicChromaticIndex,
                       const std::array<juce::String, 12>& ipnNames);

    // ── Callbacks ────────────────────────────────────────────────────────
    std::function<void (double cents)> onDrag;
    std::function<void (double cents)> onDragEnd;
    std::function<void()> onGestureStart;
    std::function<void()> onGestureEnd;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    static constexpr double kMinCents = -700.0;
    static constexpr double kMaxCents =  700.0;
    static constexpr float kKnobRadius = 14.0f;
    static constexpr float kKnobCentre = 18.0f;
    static constexpr int kKnobSize = 36;

    double currentCents = 0.0;
    double currentHz    = 440.0;
    double defaultHz    = 440.0;
    int    tonicChromatic = -1;
    std::array<juce::String, 12> ipnNames;

    // Hz input
    juce::TextEditor hzInput;
    void onHzInputReturn();

    // Semitone buttons
    juce::TextButton minusBtn { juce::CharPointer_UTF8 ("\xe2\x88\x92") };  // −
    juce::TextButton plusBtn  { "+" };

    // Drag state
    bool   isDragging    = false;
    double dragStartCents = 0.0;
    int    dragStartY    = 0;

    // ── Helpers ───────────────────────────────────────────────────────────
    juce::Rectangle<int> getKnobBounds() const;
    void drawArcKnob (juce::Graphics& g, juce::Rectangle<float> bounds);
    juce::String getTranspositionLabel() const;
    double centsToAngle (double cents) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferenceFreqControl)
};
