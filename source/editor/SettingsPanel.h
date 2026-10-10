#pragma once
#include "TanghimTheme.h"
#include <functional>

/**
 * Minimal Settings panel, shown as a modal overlay from the Settings button.
 *
 * Deliberately small: right now it holds only the auto-update preference. Other
 * user preferences (oscillator volume, default tuning system, the MIDI preset
 * device currently in the status bar) can move here as they are needed — see TODO.md.
 */
class SettingsPanel : public juce::Component
{
public:
    SettingsPanel();

    void setAutoUpdateCheck (bool enabled);
    /** Status line under the checkbox, e.g. "Tanghīm 0.9.2 is available". */
    void setUpdateStatus (const juce::String& text, bool isUpdateAvailable);
    void setCheckInProgress (bool busy);

    std::function<void (bool)> onAutoUpdateCheckChanged;
    std::function<void()>      onCheckNow;
    std::function<void()>      onOpenRelease;
    std::function<void()>      onClose;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    juce::ToggleButton autoUpdateToggle { "Check for updates automatically" };
    juce::TextButton   checkNowButton   { "Check Now" };
    juce::TextButton   openReleaseButton { "Open Release Page" };
    juce::TextButton   closeButton      { "Close" };
    juce::Label        titleLabel;
    juce::Label        statusLabel;
    juce::Label        versionLabel;

    juce::Rectangle<int> panelBounds;   // the card; clicks outside dismiss

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};
