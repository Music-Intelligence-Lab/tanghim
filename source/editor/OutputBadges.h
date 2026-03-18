#pragma once
#include "TanghimTheme.h"
#include <functional>

/**
 * Native JUCE replacement for OutputModeSelector.tsx — toggle badges (Osc, Hept)
 * and read-only status badges (MTS-ESP, MPE, Mono PB).
 */
class OutputBadges : public juce::Component
{
public:
    OutputBadges();

    void setOscillatorEnabled (bool enabled);
    void setHeptEnabled (bool enabled);
    void setHasMaqam (bool hasMaqam);
    void setMtsStatus (bool isTransmitter, int nativeCount, int mpeCount, int monoPbCount);

    std::function<void (bool)> onOscToggle;
    std::function<void (bool)> onHeptToggle;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    bool oscEnabled  = false;
    bool heptEnabled = false;
    bool hasMaqam    = false;
    bool isMtsTransmitter = false;
    int  mtsNativeCount = 0;
    int  mpeCount       = 0;
    int  monoPbCount    = 0;

    struct BadgeInfo
    {
        juce::String label;
        juce::Colour colour;
        bool active;
        bool toggleable;
        int count;       // -1 for toggles, >= 0 for count displays
        juce::Rectangle<int> bounds;
    };

    std::vector<BadgeInfo> badges;
    void rebuildBadges();
    void drawBadge (juce::Graphics& g, const BadgeInfo& badge);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputBadges)
};
