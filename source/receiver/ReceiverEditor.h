#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class ReceiverProcessor;

class ReceiverEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit ReceiverEditor (ReceiverProcessor&);
    ~ReceiverEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updatePbRangeVisibility();

    ReceiverProcessor& processor;

    juce::Label titleLabel;
    juce::Label modeLabel;
    juce::Label pbRangeLabel;
    juce::Label statusLabel;
    juce::Label versionLabel;

    juce::ComboBox modeCombo;
    juce::Slider   mpePbRangeSlider;
    juce::Slider   monoPbRangeSlider;

    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboAttachment>  modeAttachment;
    std::unique_ptr<SliderAttachment> mpePbRangeAttachment;
    std::unique_ptr<SliderAttachment> monoPbRangeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReceiverEditor)
};
