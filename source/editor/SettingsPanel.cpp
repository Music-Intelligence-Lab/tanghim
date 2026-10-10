#include "SettingsPanel.h"

SettingsPanel::SettingsPanel()
{
    setAlwaysOnTop (true);
    setWantsKeyboardFocus (true);

    titleLabel.setText ("Settings", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (16.0f).withStyle ("Bold")));
    titleLabel.setColour (juce::Label::textColourId, Theme::text);
    addAndMakeVisible (titleLabel);

    versionLabel.setText (juce::String ("Tanghim ") + JucePlugin_VersionString,
                          juce::dontSendNotification);
    versionLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    versionLabel.setColour (juce::Label::textColourId, Theme::textMuted);
    addAndMakeVisible (versionLabel);

    autoUpdateToggle.setColour (juce::ToggleButton::textColourId, Theme::text);
    autoUpdateToggle.setColour (juce::ToggleButton::tickColourId, Theme::gold);
    autoUpdateToggle.setColour (juce::ToggleButton::tickDisabledColourId, Theme::border);
    autoUpdateToggle.onClick = [this]
    {
        if (onAutoUpdateCheckChanged)
            onAutoUpdateCheckChanged (autoUpdateToggle.getToggleState());
    };
    addAndMakeVisible (autoUpdateToggle);

    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    statusLabel.setColour (juce::Label::textColourId, Theme::textMuted);
    addAndMakeVisible (statusLabel);

    auto styleButton = [] (juce::TextButton& b, juce::Colour text)
    {
        b.setColour (juce::TextButton::buttonColourId, Theme::surface2);
        b.setColour (juce::TextButton::textColourOffId, text);
    };
    styleButton (checkNowButton,    Theme::text);
    styleButton (openReleaseButton, Theme::gold);
    styleButton (closeButton,       Theme::text);

    checkNowButton.onClick    = [this] { if (onCheckNow)     onCheckNow(); };
    openReleaseButton.onClick = [this] { if (onOpenRelease)  onOpenRelease(); };
    closeButton.onClick       = [this] { if (onClose)        onClose(); };

    addAndMakeVisible (checkNowButton);
    addAndMakeVisible (openReleaseButton);
    addAndMakeVisible (closeButton);

    openReleaseButton.setVisible (false);   // only when an update is available
}

void SettingsPanel::setAutoUpdateCheck (bool enabled)
{
    autoUpdateToggle.setToggleState (enabled, juce::dontSendNotification);
}

void SettingsPanel::setUpdateStatus (const juce::String& text, bool isUpdateAvailable)
{
    statusLabel.setText (text, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId,
                           isUpdateAvailable ? Theme::gold : Theme::textMuted);
    openReleaseButton.setVisible (isUpdateAvailable);
    resized();
}

void SettingsPanel::setCheckInProgress (bool busy)
{
    checkNowButton.setEnabled (! busy);
    checkNowButton.setButtonText (busy ? "Checking…" : "Check Now");
}

void SettingsPanel::paint (juce::Graphics& g)
{
    // Scrim over the editor
    g.fillAll (juce::Colours::black.withAlpha (0.55f));

    g.setColour (Theme::surface);
    g.fillRoundedRectangle (panelBounds.toFloat(), 6.0f);
    g.setColour (Theme::border);
    g.drawRoundedRectangle (panelBounds.toFloat().reduced (0.5f), 6.0f, 1.0f);
}

void SettingsPanel::resized()
{
    const int w = 380, h = 190;
    panelBounds = { (getWidth() - w) / 2, (getHeight() - h) / 2, w, h };

    auto area = panelBounds.reduced (16);

    auto titleRow = area.removeFromTop (22);
    titleLabel.setBounds (titleRow.removeFromLeft (120));
    versionLabel.setBounds (titleRow);
    versionLabel.setJustificationType (juce::Justification::centredRight);

    area.removeFromTop (14);
    autoUpdateToggle.setBounds (area.removeFromTop (24));

    area.removeFromTop (6);
    statusLabel.setBounds (area.removeFromTop (18));

    auto buttons = area.removeFromBottom (26);
    closeButton.setBounds (buttons.removeFromRight (70));
    buttons.removeFromRight (6);
    if (openReleaseButton.isVisible())
    {
        openReleaseButton.setBounds (buttons.removeFromRight (130));
        buttons.removeFromRight (6);
    }
    checkNowButton.setBounds (buttons.removeFromRight (90));
}

bool SettingsPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onClose) onClose();
        return true;
    }
    return false;
}

void SettingsPanel::mouseDown (const juce::MouseEvent& e)
{
    // Click outside the card dismisses, matching the other popups in the editor
    if (! panelBounds.contains (e.getPosition()) && onClose)
        onClose();
}
