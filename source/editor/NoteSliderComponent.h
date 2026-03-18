#pragma once
#include "TanghimTheme.h"
#include "../model/ActiveTuningState.h"

/**
 * Native JUCE replacement for NoteSlider.tsx — a single vertical tuning slider
 * for one MIDI note position.
 *
 * Handles:
 * - Vertical drag for continuous cents adjustment (±150 cents)
 * - Shift+drag for per-note cents override
 * - Snap marker clicks for variant selection
 * - Multiple visual states: normal, maqam degree, modified, per-note override, MIDI hit
 * - Piano key indicator bar
 * - IPN label, solfège, PAO name display
 */
class NoteSliderComponent : public juce::Component
{
public:
    NoteSliderComponent();

    // ── State setters (called by NoteSliderBank before repaint) ──────────
    void setSlotData (const ChromaticNoteVariants& slot,
                      int chromaticIdx, int midiNote, int effectiveIdx);
    void setMaqamState (bool isDegree, bool isDegreeEquiv,
                        bool isTonic, bool isTonicEquiv,
                        bool isModified, int maqamVariantIdx);
    void setHeptState (bool isMuted, const juce::String& sourceKey, bool isWhiteKey);
    void setLabels (const juce::String& ipn, const juce::String& sol,
                    const juce::String& pao);
    void setOverrideState (bool hasVariantOverride, bool hasPerNoteCents,
                           double perNoteCentsVal);
    void setCentsOverride (double cents);
    void clearCentsOverride();
    void setMidiHit (bool hit);

    // ── Callbacks ─────────────────────────────────────────────────────────
    std::function<void (int chromaticIndex, int variantIndex)> onVariantSelect;
    std::function<void (int chromaticIndex, double cents)> onCentsDrag;
    std::function<void (int chromaticIndex, double cents)> onCentsDragEnd;
    std::function<void (int midiNote, double cents)> onNoteCentsDrag;
    std::function<void (int midiNote, double cents)> onNoteCentsDragEnd;
    std::function<void (int chromaticIndex)> onGestureStart;
    std::function<void (int chromaticIndex)> onGestureEnd;

    // ── Component overrides ───────────────────────────────────────────────
    void paint (juce::Graphics& g) override;
    juce::MouseCursor getMouseCursor() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    // ── Slot data ─────────────────────────────────────────────────────────
    std::vector<PitchClass> variants;
    int    chromaticIndex     = 0;
    int    midiNote           = 60;
    int    effectiveIndex     = 0;
    double slotCentsOffset    = 0.0;

    // ── Maqam state ───────────────────────────────────────────────────────
    bool   isMaqamDegree      = false;
    bool   isMaqamDegreeEquiv = false;
    bool   isMaqamTonic       = false;
    bool   isMaqamTonicEquiv  = false;
    bool   isModified         = false;
    int    maqamVariantIndex  = -1;

    // ── Hept state ────────────────────────────────────────────────────────
    bool   isHeptMuted        = false;
    bool   isWhiteKey         = true;
    juce::String heptSourceKey;

    // ── Override state ────────────────────────────────────────────────────
    bool   hasVariantOverride     = false;
    bool   hasPerNoteCentsOverride = false;
    double perNoteCentsValue      = 0.0;
    bool   hasCentsOverride       = false;
    double centsOverrideValue     = 0.0;

    // ── Display labels ────────────────────────────────────────────────────
    juce::String ipnLabel;
    juce::String solfegeLabel;
    juce::String paoNameLabel;

    // ── Visual state ──────────────────────────────────────────────────────
    bool   isMidiHit          = false;

    // ── Drag state ────────────────────────────────────────────────────────
    bool   isDragging         = false;
    bool   isPerNoteDrag      = false;

    // ── Helpers ───────────────────────────────────────────────────────────
    double getEffectiveCents() const;
    juce::Rectangle<int> getTrackBounds() const;
    double yToCents (int mouseY) const;
    juce::Colour getThumbColour() const;
    juce::Colour getThumbGlowColour() const;
    juce::Colour getLabelColour() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteSliderComponent)
};
