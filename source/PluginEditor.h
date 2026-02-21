#pragma once
#include "PluginProcessor.h"
#include "receiver/ReceiverRegistry.h"
#include "engine/MidiFileGenerator.h"
#include <juce_gui_extra/juce_gui_extra.h>

class NativeBridge;

/**
 * Native MIDI drag button that overlays the WebView.
 * Enables drag-and-drop of maqam MIDI files to DAW timeline.
 */
class MidiDragButton : public juce::Component
{
public:
    MidiDragButton (ArabicMaqamTunerProcessor& p) : processor (p) {}

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (isMouseOver() ? juce::Colour (0xffe8b339) : juce::Colour (0xff1a1a2e));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (isMouseOver() ? juce::Colour (0xff1a1a2e) : juce::Colour (0xffe8b339));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
        g.setFont (11.0f);
        g.drawText ("MIDI", getLocalBounds(), juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseDown (const juce::MouseEvent&) override
    {
        // Delete previous temp file before creating new one
        if (tempMidiFile.existsAsFile())
            tempMidiFile.deleteFile();

        // Prepare MIDI file on mouse down for responsive drag
        prepareMidiFile();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (tempMidiFile.existsAsFile() && e.getDistanceFromDragStart() > 4)
        {
            // Don't delete on completion - DAW may still be reading the file
            // File will be cleaned up on next mouseDown or app exit
            juce::DragAndDropContainer::performExternalDragDropOfFiles (
                { tempMidiFile.getFullPathName() }, false, this, nullptr);
        }
    }

private:
    void prepareMidiFile();

    ArabicMaqamTunerProcessor& processor;
    juce::File tempMidiFile;
};

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

    // Native status bar components
    MidiDragButton     midiDragButton;
    juce::String       lastMaqamId;
    juce::String       lastStatusMessage;
    juce::TextButton   updatesButton { juce::CharPointer_UTF8 ("\xe2\x86\xbb Updates") }; // ↻ Updates

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ArabicMaqamTunerEditor)
};
