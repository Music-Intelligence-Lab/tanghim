#pragma once
#include "PluginProcessor.h"
#include "receiver/ReceiverRegistry.h"
#include <juce_gui_extra/juce_gui_extra.h>

class NativeBridge;

/**
 * Plugin editor. Hosts a full-window WebBrowserComponent that renders
 * the React UI.
 *
 * Debug builds point to the Vite dev server (hot reload).
 * Release builds serve the bundled dist/ from BinaryData via a resource provider.
 */
class ArabicMaqamTunerEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit ArabicMaqamTunerEditor (ArabicMaqamTunerProcessor&);
    ~ArabicMaqamTunerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called by NativeBridge to push state changes to the WebView. */
    void emitTuningStateChanged();
    void emitTuningSystemsLoaded();
    void emitMaqamListLoaded();
    void emitStatusMessage (const juce::String& msg);

private:
    void timerCallback() override;

    ArabicMaqamTunerProcessor& processor;

    // bridge must be declared before browser: native functions are registered
    // in Options during browser construction, so bridge must exist first.
    std::unique_ptr<NativeBridge>              bridge;
    std::unique_ptr<juce::WebBrowserComponent> browser;

    /** Serve a resource from BinaryData (release builds). */
    juce::WebBrowserComponent::Resource getResourceForPath (const juce::String& path);

    // MTS-ESP status polling (~2Hz via frame counter in 30Hz timer)
    int            mtsStatusFrameCounter = 0;
    int            lastMtsTotal = -1;
    bool           lastIsMtsTransmitter = false;
    ReceiverCounts lastReceiverCounts;
    int            staleCleanupCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArabicMaqamTunerEditor)
};
