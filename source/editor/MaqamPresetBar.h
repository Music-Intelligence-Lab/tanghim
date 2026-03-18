#pragma once
#include "TanghimTheme.h"
#include "../model/MaqamPreset.h"
#include <array>
#include <functional>

/**
 * Native JUCE replacement for MaqamPresetBar.tsx + MaqamPresetButton.tsx.
 * 4×2 grid of 8 presets with save/apply/clear/MIDI-learn.
 */
class MaqamPresetBar : public juce::Component
{
public:
    MaqamPresetBar();

    void setPresets (const std::array<MaqamPreset, 16>& presets);
    void setActivePresetIndex (int index);
    void setMidiLearnTarget (int presetIdx);
    void setMidiPresetNotes (const std::array<int, 16>& notes);
    void setPresetCompatible (const std::array<bool, 8>& compatible);

    // Callbacks
    std::function<void (int presetIndex)> onApplyPreset;
    std::function<void (int presetIndex)> onSavePreset;
    std::function<void (int presetIndex)> onClearPreset;
    std::function<void (int presetIndex)> onMidiLearnStart;
    std::function<void()>                 onMidiLearnCancel;
    std::function<void (int presetIndex)> onMidiNoteClear;
    std::function<void (int presetIndex)> onDeactivatePreset;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    std::array<MaqamPreset, 16> presets;
    int activePresetIndex = -1;
    int midiLearnTarget   = -1;
    std::array<int, 16>  midiPresetNotes;
    std::array<bool, 8>  compatible;

    struct PresetButton
    {
        juce::Rectangle<int> bounds;
        juce::Rectangle<int> clearBounds;
        juce::Rectangle<int> midiBadgeBounds;
    };
    std::array<PresetButton, 8> buttons;

    void drawPresetButton (juce::Graphics& g, int index);
    static juce::String midiToNoteName (int note);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MaqamPresetBar)
};
