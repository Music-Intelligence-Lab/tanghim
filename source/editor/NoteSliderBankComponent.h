#pragma once
#include "NoteSliderComponent.h"
#include <array>
#include <set>
#include <map>

/**
 * Native JUCE replacement for NoteSliderBank.tsx — scrollable bank of 128 note sliders.
 *
 * Uses a Viewport-like approach: only repaints visible sliders, positioned
 * at fixed 68px intervals. Scroll position (startMidi) is a fractional MIDI note.
 */
class NoteSliderBankComponent : public juce::Component
{
public:
    NoteSliderBankComponent();

    // ── Configuration (call before repaint) ──────────────────────────────
    void setTuningState (const ActiveTuningState& state);
    void setScrollPosition (double startMidi);
    void setMaqamInfo (const std::set<int>& degreeIndices,
                       int tonicIndex, int tonicMidi,
                       const std::set<int>& modifiedSlots,
                       const std::map<int, juce::String>& degreePaoNames);
    void setDegreeIpnMap (const std::array<juce::String, 12>& ipnMap);
    void setDegreeSolfegeMap (const std::array<juce::String, 12>& solMap);
    void setNoteNames (const std::map<int, std::map<int, juce::String>>& names);
    void setHeptEnabled (bool enabled);
    void updateSlotCents (int chromaticIndex, double cents);
    void updateMidiActivity (const std::vector<int>& onsVec, const std::vector<int>& offsVec);

    // ── Callbacks (forwarded from individual sliders) ─────────────────────
    std::function<void (int chromaticIndex, int variantIndex)> onVariantSelect;
    std::function<void (int chromaticIndex, double cents)> onCentsDrag;
    std::function<void (int chromaticIndex, double cents)> onCentsDragEnd;
    std::function<void (int midiNote, double cents)> onNoteCentsDrag;
    std::function<void (int midiNote, double cents)> onNoteCentsDragEnd;
    std::function<void (int chromaticIndex)> onGestureStart;
    std::function<void (int chromaticIndex)> onGestureEnd;

    /** Returns visible slider count based on current width. */
    int getVisibleSliderCount() const;

    void resized() override;

private:
    // All 128 slider components
    std::array<NoteSliderComponent, 128> sliders;

    // Current state
    ActiveTuningState tuningState;
    double scrollPosition = 48.0;

    // Maqam state
    std::set<int> maqamDegreeIndices;
    int maqamTonicIndex = -1;
    int maqamTonicMidi  = -1;
    std::set<int> modifiedSlots;
    std::map<int, juce::String> maqamDegreePaoNames;

    // Display maps
    std::array<juce::String, 12> degreeIpnMap;
    std::array<juce::String, 12> degreeSolfegeMap;
    std::map<int, std::map<int, juce::String>> noteNames;  // ci -> octave -> PAO name

    bool heptEnabled = false;

    // Hept info (computed from degree indices + IPN map)
    std::set<int> heptMuted;
    std::map<int, juce::String> heptSourceKeyMap;

    // Per-slot cents overrides (from DAW automation)
    std::array<double, 12> slotCentsOverrides;
    std::array<bool, 12>   hasSlotCentsOverride;

    // MIDI activity (per-note hit state)
    std::array<bool, 128> midiHitState;

    void updateVisibleSliders();
    void computeHeptInfo();
    void setupSliderCallbacks (NoteSliderComponent& slider);

    static constexpr const char* IPN_NAMES[12] = {
        "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
    };
    static constexpr int BLACK_KEYS[5] = { 1, 3, 6, 8, 10 };
    static constexpr int WHITE_KEY_INDICES[7] = { 0, 2, 4, 5, 7, 9, 11 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteSliderBankComponent)
};
