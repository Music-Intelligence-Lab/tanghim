#include "ReceiverEditor.h"
#include "ReceiverProcessor.h"

ReceiverEditor::ReceiverEditor (ReceiverProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setSize (360, 210);

    // Title
    titleLabel.setText ("Tanghim Receiver", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    // Mode selector
    modeLabel.setText ("Mode:", juce::dontSendNotification);
    addAndMakeVisible (modeLabel);

    modeCombo.addItem ("MPE", 1);
    modeCombo.addItem ("Pitch Bend", 2);
    addAndMakeVisible (modeCombo);
    modeAttachment = std::make_unique<ComboAttachment> (processor.apvts, "mode", modeCombo);
    modeCombo.onChange = [this] { updatePbRangeVisibility(); };

    // PB Range label
    pbRangeLabel.setText ("PB Range:", juce::dontSendNotification);
    addAndMakeVisible (pbRangeLabel);

    // MPE PB Range slider
    mpePbRangeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mpePbRangeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    mpePbRangeSlider.setTextValueSuffix (" st");
    addAndMakeVisible (mpePbRangeSlider);
    mpePbRangeAttachment = std::make_unique<SliderAttachment> (processor.apvts, "mpePbRange", mpePbRangeSlider);

    // Mono PB Range slider
    monoPbRangeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    monoPbRangeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
    monoPbRangeSlider.setTextValueSuffix (" st");
    addAndMakeVisible (monoPbRangeSlider);
    monoPbRangeAttachment = std::make_unique<SliderAttachment> (processor.apvts, "monoPbRange", monoPbRangeSlider);

    // Status
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (statusLabel);

    // Version
    versionLabel.setText (juce::String ("v") + PLUGIN_VERSION, juce::dontSendNotification);
    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setColour (juce::Label::textColourId, juce::Colours::grey.withAlpha (0.6f));
    versionLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (versionLabel);

    updatePbRangeVisibility();
    startTimerHz (5);
}

void ReceiverEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void ReceiverEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    titleLabel.setBounds (area.removeFromTop (30));
    area.removeFromTop (8);

    // Mode row
    auto modeRow = area.removeFromTop (24);
    modeLabel.setBounds (modeRow.removeFromLeft (70));
    modeCombo.setBounds (modeRow);
    area.removeFromTop (8);

    // PB Range row
    auto pbRow = area.removeFromTop (24);
    pbRangeLabel.setBounds (pbRow.removeFromLeft (70));
    mpePbRangeSlider.setBounds (pbRow);
    monoPbRangeSlider.setBounds (pbRow);
    area.removeFromTop (12);

    // Status
    statusLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (4);

    // Version
    versionLabel.setBounds (area.removeFromTop (16));
}

void ReceiverEditor::timerCallback()
{
    // Read thread-safe status cached by processBlock (no MTS-ESP calls here)
    if (processor.isConnectedToMaster())
    {
        juce::String name = processor.getScaleName();
        statusLabel.setText ("Connected \u2014 \"" + name + "\"", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
    }
    else
    {
        statusLabel.setText ("No transmitter found", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    }
}

void ReceiverEditor::updatePbRangeVisibility()
{
    const bool isMpe = (modeCombo.getSelectedId() == 1);
    mpePbRangeSlider.setVisible (isMpe);
    monoPbRangeSlider.setVisible (! isMpe);
}
