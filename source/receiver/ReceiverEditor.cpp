#include "ReceiverEditor.h"
#include "ReceiverProcessor.h"
#include "BuildTimestamp.h"

// Theme colours come from "../editor/TanghimTheme.h" (included via ReceiverEditor.h)

//==============================================================================
ReceiverLookAndFeel::ReceiverLookAndFeel()
{
    // Window
    setColour (juce::ResizableWindow::backgroundColourId, Theme::bg);

    // Labels
    setColour (juce::Label::textColourId, Theme::text);

    // ComboBox (kept for completeness)
    setColour (juce::ComboBox::backgroundColourId,     Theme::surface);
    setColour (juce::ComboBox::textColourId,            Theme::text);
    setColour (juce::ComboBox::outlineColourId,         Theme::border);
    setColour (juce::ComboBox::arrowColourId,           Theme::textMuted);
    setColour (juce::ComboBox::focusedOutlineColourId,  Theme::accent);

    // PopupMenu
    setColour (juce::PopupMenu::backgroundColourId,            Theme::surface);
    setColour (juce::PopupMenu::textColourId,                  Theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent);
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

    // Buttons
    setColour (juce::TextButton::buttonColourId,   Theme::surface);
    setColour (juce::TextButton::buttonOnColourId,  Theme::accent);
    setColour (juce::TextButton::textColourOffId,   Theme::text);
    setColour (juce::TextButton::textColourOnId,    juce::Colours::white);

    // TextEditor (used in editable labels)
    setColour (juce::TextEditor::backgroundColourId,      Theme::surface);
    setColour (juce::TextEditor::textColourId,             Theme::text);
    setColour (juce::TextEditor::outlineColourId,          Theme::border);
    setColour (juce::TextEditor::focusedOutlineColourId,   Theme::accent);
    setColour (juce::TextEditor::highlightColourId,        Theme::accent.withAlpha (0.4f));

    // Caret
    setColour (juce::CaretComponent::caretColourId, Theme::accent);
}

//==============================================================================
ReceiverEditor::ReceiverEditor (ReceiverProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);
    setSize (360, 260);

    // ── Title ─────────────────────────────────────────────────────────────
    titleLabel.setText ("Tanghim Receiver", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, Theme::text);
    addAndMakeVisible (titleLabel);

    // ── Connection status ─────────────────────────────────────────────────
    connectionLabel.setFont (juce::FontOptions (12.0f));
    connectionLabel.setJustificationType (juce::Justification::centred);
    connectionLabel.setColour (juce::Label::textColourId, Theme::textMuted);
    addAndMakeVisible (connectionLabel);

    // ── Mode toggle buttons ───────────────────────────────────────────────
    auto setupModeButton = [this] (juce::TextButton& btn)
    {
        btn.setClickingTogglesState (true);
        btn.setRadioGroupId (1);
        addAndMakeVisible (btn);
    };

    setupModeButton (mpeButton);
    setupModeButton (monoPbButton);

    mpeButton.onClick = [this]
    {
        if (mpeButton.getToggleState())
        {
            auto* param = processor.apvts.getParameter ("mode");
            param->beginChangeGesture();
            param->setValueNotifyingHost (0.0f);
            param->endChangeGesture();
            updatePbDisplay();
        }
    };

    monoPbButton.onClick = [this]
    {
        if (monoPbButton.getToggleState())
        {
            auto* param = processor.apvts.getParameter ("mode");
            param->beginChangeGesture();
            param->setValueNotifyingHost (1.0f);
            param->endChangeGesture();
            updatePbDisplay();
        }
    };

    // ── PB Range controls ─────────────────────────────────────────────────
    pbLabel.setText ("PB Range", juce::dontSendNotification);
    pbLabel.setJustificationType (juce::Justification::centred);
    pbLabel.setColour (juce::Label::textColourId, Theme::textMuted);
    pbLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (pbLabel);

    // Decrease button
    pbDecButton.setButtonText (juce::String::charToString (0x2212));  // Unicode minus sign U+2212
    pbDecButton.setColour (juce::TextButton::buttonColourId, Theme::surface);
    pbDecButton.setColour (juce::TextButton::textColourOffId, Theme::text);
    pbDecButton.onClick = [this]
    {
        auto* param = dynamic_cast<juce::AudioParameterInt*> (
            processor.apvts.getParameter (isMpeMode() ? "mpePbRange" : "monoPbRange"));
        if (param != nullptr)
            setPbValue (param->get() - 1);
    };
    addAndMakeVisible (pbDecButton);

    // Increase button
    pbIncButton.setButtonText ("+");
    pbIncButton.setColour (juce::TextButton::buttonColourId, Theme::surface);
    pbIncButton.setColour (juce::TextButton::textColourOffId, Theme::text);
    pbIncButton.onClick = [this]
    {
        auto* param = dynamic_cast<juce::AudioParameterInt*> (
            processor.apvts.getParameter (isMpeMode() ? "mpePbRange" : "monoPbRange"));
        if (param != nullptr)
            setPbValue (param->get() + 1);
    };
    addAndMakeVisible (pbIncButton);

    // Editable value label
    pbValueLabel.setJustificationType (juce::Justification::centred);
    pbValueLabel.setFont (juce::FontOptions (15.0f));
    pbValueLabel.setColour (juce::Label::textColourId, Theme::text);
    pbValueLabel.setColour (juce::Label::backgroundColourId, Theme::surface);
    pbValueLabel.setColour (juce::Label::outlineColourId, Theme::border);
    pbValueLabel.setEditable (true, true, false);
    pbValueLabel.onTextChange = [this]
    {
        int val = pbValueLabel.getText().getIntValue();
        setPbValue (val);
    };
    addAndMakeVisible (pbValueLabel);

    // ── Tuning info labels ────────────────────────────────────────────────
    tuningSystemLabel.setFont (juce::FontOptions (14.0f));
    tuningSystemLabel.setJustificationType (juce::Justification::centred);
    tuningSystemLabel.setColour (juce::Label::textColourId, Theme::tuningText);
    addAndMakeVisible (tuningSystemLabel);

    maqamInfoLabel.setFont (juce::FontOptions (14.0f));
    maqamInfoLabel.setJustificationType (juce::Justification::centred);
    maqamInfoLabel.setColour (juce::Label::textColourId, Theme::text);
    addAndMakeVisible (maqamInfoLabel);

    // ── Version ───────────────────────────────────────────────────────────
    versionLabel.setSourceText (juce::String ("v") + PLUGIN_VERSION + " (" + BUILD_TIMESTAMP + ")");
    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setColour (juce::Label::textColourId, Theme::textMuted.withAlpha (0.6f));
    versionLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (versionLabel);

    updateModeButtons();
    updatePbDisplay();
    startTimerHz (5);
}

ReceiverEditor::~ReceiverEditor()
{
    setLookAndFeel (nullptr);
}

void ReceiverEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);

    auto area = getLocalBounds().reduced (16);
    g.setColour (Theme::border);

    // Separator below connection status
    g.drawHorizontalLine (area.getY() + 48, (float) area.getX(), (float) area.getRight());

    // Separator below PB range
    g.drawHorizontalLine (area.getY() + 150, (float) area.getX(), (float) area.getRight());
}

void ReceiverEditor::resized()
{
    auto area = getLocalBounds().reduced (16);

    // ── Title + connection status ─────────────────────────────────────────
    titleLabel.setBounds (area.removeFromTop (26));
    area.removeFromTop (2);
    connectionLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (6);  // space before separator

    // ── Mode toggle buttons (centred) ─────────────────────────────────────
    area.removeFromTop (6);  // space after separator
    auto modeRow = area.removeFromTop (28);
    const int buttonW = 140;
    const int gap = 8;
    const int totalW = buttonW * 2 + gap;
    const int x0 = modeRow.getX() + (modeRow.getWidth() - totalW) / 2;
    mpeButton.setBounds (x0, modeRow.getY(), buttonW, 28);
    monoPbButton.setBounds (x0 + buttonW + gap, modeRow.getY(), buttonW, 28);
    area.removeFromTop (10);

    // ── PB Range row (label above, controls centred) ──────────────────────
    pbLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (4);

    auto pbRow = area.removeFromTop (28);
    const int btnSize = 28;
    const int valueW = 56;
    const int pbGap = 6;
    const int pbTotalW = btnSize + pbGap + valueW + pbGap + btnSize;
    const int pbX0 = pbRow.getX() + (pbRow.getWidth() - pbTotalW) / 2;
    pbDecButton.setBounds (pbX0, pbRow.getY(), btnSize, btnSize);
    pbValueLabel.setBounds (pbX0 + btnSize + pbGap, pbRow.getY(), valueW, btnSize);
    pbIncButton.setBounds (pbX0 + btnSize + pbGap + valueW + pbGap, pbRow.getY(), btnSize, btnSize);
    area.removeFromTop (10);  // space before separator

    // ── Tuning info ───────────────────────────────────────────────────────
    area.removeFromTop (6);  // space after separator
    tuningSystemLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (2);
    maqamInfoLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (4);

    // ── Version ───────────────────────────────────────────────────────────
    versionLabel.setBounds (area.removeFromTop (16));
}

void ReceiverEditor::timerCallback()
{
    // Sync mode buttons from APVTS (handles DAW automation)
    updateModeButtons();
    updatePbDisplay();

    // Connection status + tuning info
    if (processor.isConnectedToMaster())
    {
        connectionLabel.setText ("Connected", juce::dontSendNotification);
        connectionLabel.setColour (juce::Label::textColourId, Theme::connected);

        juce::String scaleName = processor.getScaleName();
        // Scale name format: "tuning system\nmaqam info" or just "tuning system"
        const int newlineIdx = scaleName.indexOfChar ('\n');
        if (newlineIdx >= 0)
        {
            tuningSystemLabel.setText (scaleName.substring (0, newlineIdx), juce::dontSendNotification);
            maqamInfoLabel.setText (scaleName.substring (newlineIdx + 1), juce::dontSendNotification);
        }
        else
        {
            tuningSystemLabel.setText (scaleName, juce::dontSendNotification);
            maqamInfoLabel.setText ({}, juce::dontSendNotification);
        }
    }
    else
    {
        connectionLabel.setText ("No transmitter found", juce::dontSendNotification);
        connectionLabel.setColour (juce::Label::textColourId, Theme::textMuted);
        tuningSystemLabel.setText ({}, juce::dontSendNotification);
        maqamInfoLabel.setText ({}, juce::dontSendNotification);
    }
}

bool ReceiverEditor::isMpeMode() const
{
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter ("mode"));
    return param != nullptr && param->getIndex() == 0;
}

void ReceiverEditor::updateModeButtons()
{
    const bool mpe = isMpeMode();
    mpeButton.setToggleState (mpe, juce::dontSendNotification);
    monoPbButton.setToggleState (! mpe, juce::dontSendNotification);
}

void ReceiverEditor::updatePbDisplay()
{
    const auto* param = dynamic_cast<juce::AudioParameterInt*> (
        processor.apvts.getParameter (isMpeMode() ? "mpePbRange" : "monoPbRange"));

    if (param != nullptr)
    {
        juce::String display = juce::String (param->get()) + " st";

        // Only update if the label isn't currently being edited
        if (! pbValueLabel.isBeingEdited())
            pbValueLabel.setText (display, juce::dontSendNotification);
    }
}

void ReceiverEditor::setPbValue (int newVal)
{
    const int minVal = isMpeMode() ? 1 : 2;
    newVal = juce::jlimit (minVal, 96, newVal);
    auto* param = dynamic_cast<juce::AudioParameterInt*> (
        processor.apvts.getParameter (isMpeMode() ? "mpePbRange" : "monoPbRange"));

    if (param != nullptr)
    {
        param->beginChangeGesture();
        *param = newVal;
        param->endChangeGesture();
    }

    updatePbDisplay();
}
