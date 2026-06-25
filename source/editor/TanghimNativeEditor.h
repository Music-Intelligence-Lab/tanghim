#pragma once
#include "../PluginProcessor.h"
#include "../receiver/ReceiverRegistry.h"
#include "../engine/MidiFileGenerator.h"
#include "TanghimTheme.h"
#include "NoteSliderBankComponent.h"
#include "ReferenceFreqControl.h"
#include "OutputBadges.h"
#include "MaqamSelectorComponent.h"
#include "MaqamPresetBar.h"
#include "RangeScrollerComponent.h"
#include "SearchablePopup.h"
#include <set>
#include <map>

/**
 * Native MIDI drag button (carried over from PluginEditor.h).
 */
class MidiDragButton : public juce::Component
{
public:
    MidiDragButton (TanghimProcessor& p) : processor (p) {}

    void setActive (bool shouldBeActive)
    {
        if (active != shouldBeActive) { active = shouldBeActive; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (active)
        {
            g.setColour (isMouseOver() ? Theme::goldPreset : Theme::surface);
            g.fillRoundedRectangle (bounds, 3.0f);
            g.setColour (isMouseOver() ? Theme::surface : Theme::goldPreset);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
        }
        else
        {
            g.setColour (Theme::surface);
            g.fillRoundedRectangle (bounds, 3.0f);
            g.setColour (Theme::border);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
        }
        g.setFont (11.0f);
        g.drawText ("MIDI", getLocalBounds(), juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override { if (active) repaint(); }
    void mouseExit (const juce::MouseEvent&) override { if (active) repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    void prepareMidiFile();
    TanghimProcessor& processor;
    juce::File tempMidiFile;
    bool dragStarted = false;
    bool active = false;
};

/**
 * Custom LookAndFeel for status bar ComboBoxes.
 */
class StatusBarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StatusBarLookAndFeel();

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    int getPopupMenuBorderSize() override;
};

/**
 * Full native editor for Tanghim — replaces the WebView-based editor.
 *
 * Layout:
 * ┌──────────────────────────────────────────────────────────────┐
 * │ Top Bar                                                      │
 * │  ┌─────────────────────┐  ┌─────────────────────────────┐   │
 * │  │ TuningSystemSelector│  │ OutputBadges                │   │
 * │  │ MaqamSelector       │  │                             │   │
 * │  │ ReferenceFreqControl│  │                             │   │
 * │  └─────────────────────┘  └─────────────────────────────┘   │
 * ├──────────────────────────────────────────────────────────────┤
 * │ NoteSliderBank (flex: 1)                                     │
 * ├──────────────────────────────────────────────────────────────┤
 * │ MaqamPresetBar                                               │
 * ├──────────────────────────────────────────────────────────────┤
 * │ RangeScroller                                                │
 * ├──────────────────────────────────────────────────────────────┤
 * │ Status Bar (26px)                                            │
 * └──────────────────────────────────────────────────────────────┘
 */
class TanghimNativeEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit TanghimNativeEditor (TanghimProcessor&);
    ~TanghimNativeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // ── State sync from processor ────────────────────────────────────────
    void syncFullState();
    void syncMaqamList();
    void syncMtsStatus();

    // ── Derived state computation (moved from App.tsx) ───────────────────
    void computeDerivedState();
    void computeModifiedSlots();
    std::set<int> computeMaqamDegreeIndices() const;
    std::map<int, juce::String> buildDegreePaoNameMap() const;
    struct TonicInfo { int midi; int chromaticIndex; };
    std::optional<TonicInfo> findTonicMidi (const juce::String& tonicDisplay) const;

    TanghimProcessor& processor;
    TanghimLookAndFeel lnf;

    // ── UI Components ────────────────────────────────────────────────────
    // Menu bar (Load, Save, Settings)
    juce::TextButton loadButton   { "Load" };
    juce::TextButton saveButton   { "Save" };
    juce::TextButton settingsButton { "Settings" };

    TuningSystemSelectorComponent tuningSystemSelector;
    MaqamSelectorComponent        maqamSelector;
    ReferenceFreqControl          referenceFreqControl;
    OutputBadges                  outputBadges;
    NoteSliderBankComponent       noteSliderBank;
    MaqamPresetBar                presetBar;
    RangeScrollerComponent        rangeScroller;

    // ── Status bar ───────────────────────────────────────────────────────
    MidiDragButton     midiDragButton;
    juce::String       lastMaqamId;
    juce::TextButton   updatesButton { juce::CharPointer_UTF8 ("\xe2\x9f\xb3 Updates") };
    juce::TextButton   clearCacheButton { juce::CharPointer_UTF8 ("\xc3\x97 Clear Cache") };
    juce::Label        downloadStatusLabel;
    CopyableLabel      versionLabel;
    juce::TextButton   retryButton;
    StatusBarLookAndFeel statusBarLnF;
    juce::Label        midiPresetLabel { {}, "Preset MIDI Map Config:" };
    juce::ComboBox     midiDeviceSelector;
    juce::ComboBox     midiChannelSelector;
    juce::MidiDeviceListConnection midiDeviceListConnection;
    std::atomic<bool>  midiDevicesChanged { true };

    void setupStatusBar();
    void populateMidiDeviceList();
    void populateMidiChannelList();

    // ── File dialogs ─────────────────────────────────────────────────────
    std::unique_ptr<juce::FileChooser> fileChooser;

    // ── Polling state ────────────────────────────────────────────────────
    bool updateButtonShowingConfirmation = false;
    int  mtsStatusFrameCounter = 0;
    int  lastMtsTotal = -1;
    bool lastIsMtsTransmitter = false;
    ReceiverCounts lastReceiverCounts;
    int  staleCleanupCounter = 0;
    int  slowPollCounter = 0;

    // ── Derived maqam state ──────────────────────────────────────────────
    std::set<int>                 maqamDegreeIndices;
    std::map<int, juce::String>   degreePaoNameMap;
    int                           maqamTonicMidi  = -1;
    int                           maqamTonicIndex = -1;
    std::set<int>                 modifiedSlots;
    bool                          isMaqamModified = false;

    // ── Viewport centering state ─────────────────────────────────────────
    int  lastCenteredTonicMidi = -2;    // track when tonic changes to avoid recentering on every sync
    juce::String lastCenteredSystemId;  // track system changes

    // ── Note name map (ci -> octave -> PAO display name) ─────────────────
    std::map<int, std::map<int, juce::String>> noteNameMap;
    std::map<juce::String, int>                paoNameMap;  // PAO idName -> chromaticIndex

    void buildNoteNameMaps();
    std::map<juce::String, bool> buildCacheMap() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TanghimNativeEditor)
};
