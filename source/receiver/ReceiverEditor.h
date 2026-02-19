#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class ReceiverProcessor;

//==============================================================================
/** Custom LookAndFeel matching the Transmitter's dark navy theme. */
class ReceiverLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ReceiverLookAndFeel();
};

//==============================================================================
class ReceiverEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit ReceiverEditor (ReceiverProcessor&);
    ~ReceiverEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateModeButtons();
    void updatePbDisplay();
    void setPbValue (int newVal);
    bool isMpeMode() const;

    ReceiverProcessor& processor;
    ReceiverLookAndFeel lnf;

    juce::Label titleLabel;
    juce::Label connectionLabel;

    // Mode toggle buttons
    juce::TextButton mpeButton   { "MPE" };
    juce::TextButton monoPbButton { "Mono PB" };

    // PB Range controls
    juce::Label      pbLabel;
    juce::TextButton pbDecButton;
    juce::Label      pbValueLabel;
    juce::TextButton pbIncButton;

    // Tuning info (from MTS-ESP scale name)
    juce::Label tuningSystemLabel;
    juce::Label maqamInfoLabel;

    juce::Label versionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReceiverEditor)
};
