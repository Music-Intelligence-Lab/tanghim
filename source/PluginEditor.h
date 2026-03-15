#pragma once
#include "PluginProcessor.h"
#include "receiver/ReceiverRegistry.h"
#include "engine/MidiFileGenerator.h"
#include <juce_gui_extra/juce_gui_extra.h>

class NativeBridge;

/**
 * Custom LookAndFeel for status bar ComboBoxes to match the dark theme.
 */
class StatusBarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StatusBarLookAndFeel()
    {
        // Set colors for the popup menu
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1a2e));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe8b339));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2d2d4a));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xffe8b339));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                       int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                       juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

        // Draw small arrow
        auto arrowZone = juce::Rectangle<float> ((float) width - 14.0f, 0.0f, 10.0f, (float) height);
        juce::Path arrow;
        arrow.addTriangle (arrowZone.getX() + 1.0f, arrowZone.getCentreY() - 2.0f,
                           arrowZone.getRight() - 1.0f, arrowZone.getCentreY() - 2.0f,
                           arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (4, 0, box.getWidth() - 16, box.getHeight());
        label.setFont (juce::FontOptions (11.0f));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::FontOptions (11.0f);
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::FontOptions (12.0f);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.setColour (findColour (juce::PopupMenu::backgroundColourId));
        g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f);
        g.setColour (juce::Colour (0xff3d3d5a));
        g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 4.0f, 1.0f);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override
    {
        if (isSeparator)
        {
            auto r = area.reduced (5, 0).toFloat();
            r.removeFromTop ((float) r.getHeight() / 2.0f - 0.5f);
            g.setColour (juce::Colour (0xff3d3d5a));
            g.fillRect (r.removeFromTop (1.0f));
            return;
        }

        auto textColourToUse = textColour ? *textColour
            : findColour (isHighlighted ? juce::PopupMenu::highlightedTextColourId
                                        : juce::PopupMenu::textColourId);

        if (isHighlighted)
        {
            g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect (area);
        }

        auto r = area.reduced (6, 0);
        g.setColour (textColourToUse.withAlpha (isActive ? 1.0f : 0.5f));
        g.setFont (getPopupMenuFont());

        if (isTicked)
        {
            g.drawText (juce::CharPointer_UTF8 ("\xe2\x9c\x93"), r.removeFromLeft (16),
                        juce::Justification::centredLeft);
        }
        else
        {
            r.removeFromLeft (16);
        }

        g.drawFittedText (text, r, juce::Justification::centredLeft, 1);
    }

    int getPopupMenuBorderSize() override { return 4; }
};

/**
 * Native MIDI drag button that overlays the WebView.
 * Enables drag-and-drop of maqam MIDI files to DAW timeline.
 */
class MidiDragButton : public juce::Component
{
public:
    MidiDragButton (TanghimProcessor& p) : processor (p) {}

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

        dragStarted = false;

        // Prepare MIDI file on mouse down for responsive drag
        prepareMidiFile();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragStarted && tempMidiFile.existsAsFile() && e.getDistanceFromDragStart() > 4)
        {
            dragStarted = true;
            // Don't delete on completion - DAW may still be reading the file
            // File will be cleaned up on next mouseDown or app exit
            juce::DragAndDropContainer::performExternalDragDropOfFiles (
                { tempMidiFile.getFullPathName() }, true, this, nullptr);
        }
    }

private:
    void prepareMidiFile();

    TanghimProcessor& processor;
    juce::File tempMidiFile;
    bool dragStarted = false;
};

/**
 * Plugin editor. Hosts a full-window WebBrowserComponent that renders
 * the React UI.
 *
 * Debug builds point to the Vite dev server (hot reload).
 * Release builds serve the bundled dist/ from BinaryData via a resource provider.
 */
class TanghimEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit TanghimEditor (TanghimProcessor&);
    ~TanghimEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called by NativeBridge to push state changes to the WebView. */
    void emitTuningStateChanged();
    void emitTuningSystemsLoaded();
    void emitMaqamListLoaded();
    void emitStatusMessage (const juce::String& msg);
    void emitSlotCentsChanged (int chromaticIndex, double centsOffset);

private:
    void timerCallback() override;

    TanghimProcessor& processor;

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

    // Slow polling counter for expensive I/O ops (~0.2Hz: every 5s at 2Hz ticks)
    int            slowPollCounter = 0;

    // Native status bar components
    MidiDragButton     midiDragButton;
    juce::String       lastMaqamId;
    juce::TextButton   updatesButton { juce::CharPointer_UTF8 ("\xe2\x9f\xb3 Updates") }; // ⟳ Updates
    juce::TextButton   clearCacheButton { juce::CharPointer_UTF8 ("\xc3\x97 Clear Cache") }; // × Clear Cache

    // MIDI preset trigger configuration (MIDI Learn per-preset)
    StatusBarLookAndFeel statusBarLnF;
    juce::Label        midiPresetLabel { {}, "Preset MIDI Map Config:" };
    juce::ComboBox     midiDeviceSelector;    // Direct MIDI device for preset triggering
    juce::ComboBox     midiChannelSelector;   // Channel selector (All, 1-16)
    void setupMidiPresetControls();
    void populateMidiDeviceList();
    void populateMidiChannelList();
    void onMidiDeviceChanged();
    void onMidiChannelChanged();

    // Event-driven MIDI device list updates (CoreMIDI notifications, no polling)
    juce::MidiDeviceListConnection midiDeviceListConnection;
    std::atomic<bool>              midiDevicesChanged { true };  // true on init to populate list

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TanghimEditor)
};
