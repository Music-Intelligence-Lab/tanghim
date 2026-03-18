#include "TanghimNativeEditor.h"
#include "../model/PitchClass.h"
#include "BuildTimestamp.h"

// ══════════════════════════════════════════════════════════════════════════════
// StatusBarLookAndFeel (carried over from PluginEditor.h)
// ══════════════════════════════════════════════════════════════════════════════

StatusBarLookAndFeel::StatusBarLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, Theme::surface);
    setColour (juce::PopupMenu::textColourId, Theme::goldPreset);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::surface2);
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::goldPreset);
}

void StatusBarLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                         int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

    auto arrowZone = juce::Rectangle<float> ((float) width - 14.0f, 0.0f, 10.0f, (float) height);
    juce::Path arrow;
    arrow.addTriangle (arrowZone.getX() + 1.0f, arrowZone.getCentreY() - 2.0f,
                       arrowZone.getRight() - 1.0f, arrowZone.getCentreY() - 2.0f,
                       arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void StatusBarLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 16, box.getHeight());
    label.setFont (juce::FontOptions (11.0f));
}

juce::Font StatusBarLookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::FontOptions (11.0f); }
juce::Font StatusBarLookAndFeel::getPopupMenuFont() { return juce::FontOptions (12.0f); }

void StatusBarLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.setColour (findColour (juce::PopupMenu::backgroundColourId));
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f);
    g.setColour (Theme::border.brighter (0.3f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 4.0f, 1.0f);
}

void StatusBarLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool, const juce::String& text,
                                              const juce::String&, const juce::Drawable*, const juce::Colour* textColour)
{
    if (isSeparator) { g.setColour (Theme::border); g.fillRect (area.reduced (5, 0).withHeight (1)); return; }
    auto c = textColour ? *textColour : findColour (isHighlighted ? juce::PopupMenu::highlightedTextColourId : juce::PopupMenu::textColourId);
    if (isHighlighted) { g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId)); g.fillRect (area); }
    auto r = area.reduced (6, 0);
    g.setColour (c.withAlpha (isActive ? 1.0f : 0.5f));
    g.setFont (getPopupMenuFont());
    if (isTicked) { g.drawText (juce::CharPointer_UTF8 ("\xe2\x9c\x93"), r.removeFromLeft (16), juce::Justification::centredLeft); }
    else r.removeFromLeft (16);
    g.drawFittedText (text, r, juce::Justification::centredLeft, 1);
}

int StatusBarLookAndFeel::getPopupMenuBorderSize() { return 4; }

// ══════════════════════════════════════════════════════════════════════════════
// TanghimNativeEditor
// ══════════════════════════════════════════════════════════════════════════════

TanghimNativeEditor::TanghimNativeEditor (TanghimProcessor& p)
    : AudioProcessorEditor (p), processor (p), midiDragButton (p)
{
    setLookAndFeel (&lnf);
    setSize (Theme::kMinWidth, Theme::kMinHeight);
    setResizable (true, true);
    setResizeLimits (Theme::kMinWidth, Theme::kMinHeight, Theme::kMaxWidth, Theme::kMaxHeight);

    // ── Menu bar buttons ────────────────────────────────────────────────
    auto setupMenuBtn = [this] (juce::TextButton& btn)
    {
        btn.setColour (juce::TextButton::buttonColourId, Theme::surface2);
        btn.setColour (juce::TextButton::textColourOffId, Theme::textMuted);
        addAndMakeVisible (btn);
    };
    setupMenuBtn (loadButton);
    setupMenuBtn (saveButton);
    setupMenuBtn (settingsButton);

    loadButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Load Tanghim State",
            juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.tanghim");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    auto json = file.loadFileAsString();
                    processor.restoreStateFromJson (json);
                    syncFullState();
                }
            });
    };

    saveButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> ("Save Tanghim State",
            juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.tanghim");
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File())
                {
                    auto json = processor.buildStateJson();
                    file.replaceWithText (json);
                }
            });
    };

    // ── Top bar components ────────────────────────────────────────────────
    addAndMakeVisible (tuningSystemSelector);
    addAndMakeVisible (maqamSelector);
    addAndMakeVisible (referenceFreqControl);
    addAndMakeVisible (outputBadges);

    // ── Slider bank ───────────────────────────────────────────────────────
    addAndMakeVisible (noteSliderBank);

    // ── Preset bar ────────────────────────────────────────────────────────
    addAndMakeVisible (presetBar);

    // ── Range scroller ────────────────────────────────────────────────────
    addAndMakeVisible (rangeScroller);

    // ── Wire up callbacks ─────────────────────────────────────────────────

    // Tuning system selection — preserve active preset across switches
    tuningSystemSelector.onSelect = [this] (const juce::String& sysId, const juce::String& startNote)
    {
        // Capture active preset before clearing (React: preSystemSwitchPresetRef)
        const int presetToRestore = processor.getCurrentActivePresetIdx();

        processor.loadTuningSystem (sysId, startNote, [this, presetToRestore]
        {
            syncFullState();
            syncMaqamList();

            // Re-apply preset if compatible (React: useEffect on maqamList)
            if (presetToRestore >= 0 && presetToRestore < 8)
            {
                const auto& presets = processor.getPresets();
                const auto& preset = presets[(size_t) presetToRestore];
                if (preset.isAssigned)
                {
                    // Check compatibility with new maqam list
                    const auto& list = processor.getMaqamList();
                    bool found = false;
                    for (const auto& entry : list)
                    {
                        if (entry.maqamId == preset.maqamIdName
                            || entry.maqamId == preset.baseMaqamIdName)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (found)
                    {
                        // Unmodified presets: re-apply maqam with fresh defaults
                        // Modified presets: only valid in their original system
                        if (preset.tuningSystemId.isEmpty())
                        {
                            // Unmodified: apply maqam with fresh system defaults
                            processor.applyMaqam (preset.maqamIdName,
                                                   preset.isTransposed ? preset.pitchClassSetIndex : -1);
                            processor.setCurrentActivePresetIdx (presetToRestore);
                        }
                        // Modified presets are only compatible in their original system
                        syncFullState();
                    }
                }
            }
        });
    };

    // Maqam selection
    maqamSelector.onSelect = [this] (const juce::String& maqamId, int transIdx)
    {
        processor.applyMaqam (maqamId, transIdx);
        // applyMaqam calls notifyTuningChanged → syncFullState, but some state
        // (tonic IPN/solfège) is set after applyMaqamDegrees. Sync again explicitly.
        syncFullState();
    };

    // Reference frequency
    referenceFreqControl.onDrag = [this] (double cents) { processor.setReferenceCentsOffset (cents); };
    referenceFreqControl.onDragEnd = [this] (double cents) { processor.finalizeReferenceCentsOffset (cents); syncFullState(); };
    referenceFreqControl.onGestureStart = [this] { processor.beginRefFreqGesture(); };
    referenceFreqControl.onGestureEnd = [this] { processor.endRefFreqGesture(); };

    // Output badges
    outputBadges.onOscToggle = [this] (bool enabled)
    {
        processor.setOscillatorEnabled (enabled);
        outputBadges.setOscillatorEnabled (enabled);  // immediate visual feedback
    };
    outputBadges.onHeptToggle = [this] (bool enabled)
    {
        processor.setHeptEnabled (enabled);
        outputBadges.setHeptEnabled (enabled);  // immediate visual feedback
        noteSliderBank.setHeptEnabled (enabled);
    };

    // Slider bank callbacks
    noteSliderBank.onVariantSelect = [this] (int ci, int vi)
    {
        processor.setSliderVariant (ci, vi);
        syncFullState();
    };
    noteSliderBank.onCentsDrag = [this] (int ci, double c) { processor.setSlotCents (ci, c); };
    noteSliderBank.onCentsDragEnd = [this] (int ci, double c) { processor.finalizeSlotCents (ci, c); syncFullState(); };
    noteSliderBank.onNoteCentsDrag = [this] (int mn, double c) { processor.setNoteCents (mn, c); };
    noteSliderBank.onNoteCentsDragEnd = [this] (int mn, double c) { processor.finalizeNoteCents (mn, c); syncFullState(); };
    noteSliderBank.onGestureStart = [this] (int ci) { processor.beginSliderGesture (ci); };
    noteSliderBank.onGestureEnd = [this] (int ci) { processor.endSliderGesture (ci); };

    // Preset bar callbacks
    presetBar.onApplyPreset = [this] (int idx)
    {
        const auto& preset = processor.getPresets()[(size_t) idx];
        if (preset.tuningSystemId.isEmpty())
        {
            // Unmodified preset: apply maqam with fresh system defaults
            // (applyMaqam clears activePresetIdx, so restore it after)
            processor.applyMaqam (preset.maqamIdName,
                                   preset.isTransposed ? preset.pitchClassSetIndex : -1);
            processor.setCurrentActivePresetIdx (idx);
        }
        else
        {
            // Modified preset: restore saved slider positions/cents
            processor.applyPreset (idx);
        }
        syncFullState();
    };
    presetBar.onSavePreset = [this] (int idx)
    {
        // Only save if a maqam is currently selected (matches React behavior)
        if (processor.getCurrentMaqamId().isEmpty()) return;

        // Save current maqam state to this preset slot
        const auto& state = processor.getActiveTuningState();
        std::array<int, 12> sliderPos;
        std::array<double, 12> centsOffs;
        for (int i = 0; i < 12; ++i)
        {
            sliderPos[(size_t) i] = state.slots[(size_t) i].selectedIndex;
            centsOffs[(size_t) i] = state.slots[(size_t) i].centsOffset;
        }
        processor.assignPreset (idx,
                                processor.getCurrentMaqamId(),
                                processor.getCurrentMaqamDisplay(),
                                processor.getCurrentMaqamId(),  // baseMaqamIdName
                                processor.getCurrentTranspositionIdx() >= 0,  // isTransposed
                                processor.getCurrentTonicDisplay(),
                                processor.getCurrentTonicEnglish(),
                                processor.getCurrentTonicSolfege(),
                                processor.getCurrentTranspositionIdx(),  // pitchClassSetIndex
                                sliderPos,
                                processor.getCurrentDegreeNames(),
                                centsOffs,
                                isMaqamModified ? processor.getCurrentSystemId() : juce::String(),
                                isMaqamModified ? processor.getCurrentStartingNote() : juce::String());
        syncFullState();
    };
    presetBar.onClearPreset = [this] (int idx) { processor.clearPreset (idx); syncFullState(); };
    presetBar.onDeactivatePreset = [this] (int)
    {
        // Deactivate: clear maqam + reload system to reset sliders to defaults
        // (matches React: bridge.selectTuningSystem(systemId, startingNote))
        processor.loadTuningSystem (processor.getCurrentSystemId(),
                                     processor.getCurrentStartingNote(),
                                     [this] { syncFullState(); });
    };
    presetBar.onMidiLearnStart = [this] (int idx) { processor.startMidiLearn (idx); };
    presetBar.onMidiLearnCancel = [this] { processor.cancelMidiLearn(); };
    presetBar.onMidiNoteClear = [this] (int idx) { processor.clearMidiPresetNote (idx); };

    // Range scroller
    rangeScroller.onChange = [this] (double startMidi)
    {
        processor.setStartMidi (startMidi);
        noteSliderBank.setScrollPosition (startMidi);
    };

    // ── Status bar ────────────────────────────────────────────────────────
    setupStatusBar();

    // ── MIDI device events ────────────────────────────────────────────────
    midiDeviceListConnection = juce::MidiDeviceListConnection::make ([this]
    {
        midiDevicesChanged.store (true, std::memory_order_relaxed);
    });

    // ── Processor change callbacks (direct, no bridge serialization) ─────
    processor.onTuningStateChanged = [this] { syncFullState(); };
    processor.onTuningSystemsLoaded = [this]
    {
        tuningSystemSelector.setSystems (processor.getTuningSystems());
        tuningSystemSelector.setCurrentSelection (processor.getCurrentSystemId(),
                                                  processor.getCurrentStartingNote());
        // Update cache status icons (✓ cached, ↓ not cached)
        auto cacheMap = buildCacheMap();
        tuningSystemSelector.setCacheStatus (cacheMap);
        maqamSelector.setCacheStatus (! processor.getMaqamList().empty());
    };
    processor.onMaqamListLoaded = [this] { syncMaqamList(); };
    processor.onSlotCentsChanged = [this] (int ci, double cents)
    {
        noteSliderBank.updateSlotCents (ci, cents);
    };

    // ── Initial state sync ────────────────────────────────────────────────
    {
        tuningSystemSelector.setSystems (processor.getTuningSystems());
        tuningSystemSelector.setCurrentSelection (processor.getCurrentSystemId(),
                                                  processor.getCurrentStartingNote());
        // Initial cache status
        auto cacheMap = buildCacheMap();
        tuningSystemSelector.setCacheStatus (cacheMap);
        maqamSelector.setCacheStatus (! processor.getMaqamList().empty());
    }

    // Load the saved tuning system (like React's selectTuningSystem on mount)
    // This triggers doLoad → notifyTuningChanged → syncFullState via callback
    if (processor.getCurrentSystemId().isNotEmpty())
    {
        processor.loadTuningSystem (processor.getCurrentSystemId(),
                                    processor.getCurrentStartingNote(),
                                    [this] { syncFullState(); });
    }
    else
    {
        syncFullState();
    }

    syncMaqamList();
    syncMtsStatus();

    startTimerHz (30);
}

TanghimNativeEditor::~TanghimNativeEditor()
{
    stopTimer();
    midiDeviceListConnection.reset();
    midiDeviceSelector.setLookAndFeel (nullptr);
    midiChannelSelector.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
    processor.onTuningStateChanged  = {};
    processor.onTuningSystemsLoaded = {};
    processor.onMaqamListLoaded     = {};
    processor.onStatusMessage       = {};
    processor.onSlotCentsChanged    = {};
}

// ── State sync ────────────────────────────────────────────────────────────────

void TanghimNativeEditor::syncFullState()
{
    const auto& state = processor.getActiveTuningState();

    // Build note name maps from pitch classes
    buildNoteNameMaps();

    // Compute derived maqam state
    computeDerivedState();

    // Push to slider bank — set IPN/solfege maps BEFORE maqamInfo
    // (maqamInfo triggers computeHeptInfo which needs degreeIpnMap)
    noteSliderBank.setTuningState (state);
    noteSliderBank.setScrollPosition (processor.getCurrentStartMidi());
    noteSliderBank.setDegreeIpnMap (processor.getDegreeIpnRefs());
    noteSliderBank.setDegreeSolfegeMap (processor.getDegreeSolfegeRefs());
    noteSliderBank.setMaqamInfo (maqamDegreeIndices, maqamTonicIndex, maqamTonicMidi,
                                modifiedSlots, degreePaoNameMap);
    noteSliderBank.setNoteNames (noteNameMap);
    noteSliderBank.setHeptEnabled (processor.getHeptEnabled());

    // Reference freq control
    referenceFreqControl.setCents (processor.getReferenceCentsOffset());
    referenceFreqControl.setHzValues (processor.getReferenceCurrentHz(),
                                      processor.getReferenceDefaultHz());
    auto ipnNames = processor.getDegreeIpnRefs();
    referenceFreqControl.setTonicInfo (maqamTonicIndex >= 0 ? maqamTonicIndex
        : (findTonicMidi (processor.getReferenceNoteDisplayName())
           ? findTonicMidi (processor.getReferenceNoteDisplayName())->chromaticIndex : -1),
        ipnNames);

    // Output badges
    outputBadges.setOscillatorEnabled (processor.getOscillatorEnabled());
    outputBadges.setHeptEnabled (processor.getHeptEnabled());
    outputBadges.setHasMaqam (processor.getCurrentMaqamId().isNotEmpty());

    // Maqam selector — build transposition filter data from pitch classes
    {
        std::set<juce::String> allowedTonics;
        std::map<juce::String, int> midiOrder;
        for (const auto& pc : processor.getCurrentPitchClasses())
        {
            if (pc.noteName.isEmpty()) continue;
            if (pc.octave == 1 || pc.octave == 2)
                allowedTonics.insert (pc.noteName);
            // Use first occurrence's MIDI note for pitch ordering
            if (midiOrder.count (pc.noteName) == 0)
                midiOrder[pc.noteName] = pc.midiNoteNumber;
        }
        maqamSelector.setTranspositionFilter (allowedTonics, midiOrder);
    }
    maqamSelector.setSelectedMaqam (processor.getCurrentMaqamId(),
                                    processor.getCurrentTranspositionIdx());
    maqamSelector.setModified (isMaqamModified);
    maqamSelector.setActivePresetIndex (processor.getCurrentActivePresetIdx());

    // Preset bar — update compatibility with current maqam list
    {
        const auto& list = processor.getMaqamList();
        const auto& presets = processor.getPresets();
        std::array<bool, 8> compat;
        for (int i = 0; i < 8; ++i)
        {
            if (! presets[(size_t) i].isAssigned || list.empty())
            {
                compat[(size_t) i] = ! presets[(size_t) i].isAssigned;
                continue;
            }
            bool found = false;
            const auto& p = presets[(size_t) i];
            for (const auto& entry : list)
            {
                if (entry.maqamId == p.maqamIdName || entry.maqamId == p.baseMaqamIdName)
                {
                    // Maqam exists — also check transposition is valid
                    if (p.isTransposed && p.pitchClassSetIndex >= 0)
                        found = p.pitchClassSetIndex < (int) entry.transpositions.size();
                    else
                        found = true;
                    break;
                }
            }
            compat[(size_t) i] = found;
        }
        presetBar.setPresetCompatible (compat);
    }
    presetBar.setPresets (processor.getPresets());
    presetBar.setActivePresetIndex (processor.getCurrentActivePresetIdx());

    // Range scroller
    rangeScroller.setVisibleCount (noteSliderBank.getVisibleSliderCount());
    rangeScroller.setMaqamTonicMidi (maqamTonicMidi);

    // Center viewport only when maqam tonic or tuning system changes
    // (not on every sync — user's manual scroll position should persist)
    {
        const bool tonicChanged = maqamTonicMidi != lastCenteredTonicMidi;
        const bool systemChanged = processor.getCurrentSystemId() != lastCenteredSystemId;

        if (tonicChanged || systemChanged)
        {
            lastCenteredTonicMidi = maqamTonicMidi;
            lastCenteredSystemId = processor.getCurrentSystemId();

            const int vc = noteSliderBank.getVisibleSliderCount();

            int yegahMidi = -1;
            const auto& allPCs = processor.getCurrentPitchClasses();
            for (const auto& pc : allPCs)
            {
                if (pc.noteName == "yegah") { yegahMidi = pc.midiNoteNumber; break; }
            }
            if (yegahMidi < 0)
            {
                for (const auto& pc : allPCs)
                {
                    if (pc.pitchClassIndex == 0 && pc.octave == 1) { yegahMidi = pc.midiNoteNumber; break; }
                }
            }
            if (yegahMidi < 0) yegahMidi = 48;

            double centered;
            if (vc >= 24)
                centered = (double) yegahMidi;
            else
            {
                int centerMidi = (maqamTonicMidi >= 0) ? maqamTonicMidi : yegahMidi;
                const int padding = std::max (0, (vc - 13) / 2);
                centered = (double) (centerMidi - padding);
            }
            centered = juce::jlimit (0.0, 128.0 - vc, centered);
            processor.setStartMidi (centered);
            noteSliderBank.setScrollPosition (centered);
            rangeScroller.setStartMidi (centered);
        }
        else
        {
            rangeScroller.setStartMidi (processor.getCurrentStartMidi());
        }
    }
}

void TanghimNativeEditor::syncMaqamList()
{
    const auto& list = processor.getMaqamList();
    maqamSelector.setCacheStatus (! list.empty());
    maqamSelector.setMaqamList (list);

    // Preset compatibility check
    std::array<bool, 8> compat;
    const auto& presets = processor.getPresets();
    for (int i = 0; i < 8; ++i)
    {
        if (! presets[(size_t) i].isAssigned)
        {
            compat[(size_t) i] = true;
            continue;
        }
        // Check if maqam exists in current list
        bool found = false;
        for (const auto& entry : list)
        {
            if (entry.maqamId == presets[(size_t) i].maqamIdName
                || entry.maqamId == presets[(size_t) i].baseMaqamIdName)
            {
                found = true;
                break;
            }
        }
        compat[(size_t) i] = found;
    }
    presetBar.setPresetCompatible (compat);
}

void TanghimNativeEditor::syncMtsStatus()
{
    const int totalReceivers = processor.mtsNumReceivers();
    const bool isTx = processor.isMtsTransmitter();
    const auto rc = processor.getReceiverCounts();
    const int tanghimTotal = rc.mpeReceivers + rc.monoPbReceivers;
    const int mtsNative = std::max (0, totalReceivers - tanghimTotal);
    outputBadges.setMtsStatus (isTx, mtsNative, rc.mpeReceivers, rc.monoPbReceivers);
}

std::map<juce::String, bool> TanghimNativeEditor::buildCacheMap() const
{
    std::map<juce::String, bool> cacheMap;
    for (const auto& sys : processor.getTuningSystems())
        for (const auto& noteId : sys.startingNoteIds)
            cacheMap[sys.id + ":" + noteId] = processor.hasCachedTuningData (sys.id, noteId);
    return cacheMap;
}

// ── Derived state computation ─────────────────────────────────────────────────

void TanghimNativeEditor::buildNoteNameMaps()
{
    noteNameMap.clear();
    paoNameMap.clear();

    const auto& allPCs = processor.getCurrentPitchClasses();
    const auto& state = processor.getActiveTuningState();

    // Build PAO name → chromatic index lookup (all pitch classes, all octaves)
    for (const auto& pc : allPCs)
    {
        if (pc.noteName.isEmpty()) continue;
        int ci = chromaticIndexForIpnRef (pc.ipnReference);
        if (ci >= 0)
            paoNameMap[pc.noteName] = ci;
    }

    // Build note name map — mirrors NativeBridge::buildTuningStateJson exactly:
    // For each chromatic index, group PCs by octave, sort by cents, then use
    // effectiveVariantIndex to pick the display name from the sorted list.
    for (int ci = 0; ci < 12; ++ci)
    {
        std::map<int, std::vector<const PitchClass*>> byOctave;
        for (const auto& pc : allPCs)
        {
            if (chromaticIndexForIpnRef (pc.ipnReference) == ci
                && pc.midiNoteNumber >= 0 && pc.midiNoteNumber < 128)
            {
                const int oct = pc.midiNoteNumber / 12 - 1;
                byOctave[oct].push_back (&pc);
            }
        }

        for (auto& [oct, pcs] : byOctave)
        {
            std::sort (pcs.begin(), pcs.end(),
                       [] (const PitchClass* a, const PitchClass* b)
                       { return a->cents < b->cents; });

            const int midiNote = (oct + 1) * 12 + ci;
            const int effectiveIdx = (midiNote >= 0 && midiNote < 128)
                ? state.effectiveVariantIndex (midiNote)
                : state.slots[(size_t) ci].selectedIndex;

            const int idx = juce::jlimit (0, (int) pcs.size() - 1, effectiveIdx);
            noteNameMap[ci][oct] = pcs[(size_t) idx]->noteNameDisplay;
        }
    }
}

void TanghimNativeEditor::computeDerivedState()
{
    const auto& degreeNames = processor.getCurrentDegreeNames();

    maqamDegreeIndices = computeMaqamDegreeIndices();
    degreePaoNameMap = buildDegreePaoNameMap();

    // Find tonic MIDI
    maqamTonicMidi = -1;
    maqamTonicIndex = -1;
    if (! degreeNames.empty())
    {
        // Use tonic display name to find MIDI position
        juce::String tonicDisplay = processor.getCurrentTonicDisplay();
        if (tonicDisplay.isNotEmpty())
        {
            auto info = findTonicMidi (tonicDisplay);
            if (info)
            {
                maqamTonicMidi = info->midi;
                maqamTonicIndex = info->chromaticIndex;
            }
        }
    }

    computeModifiedSlots();
}

std::set<int> TanghimNativeEditor::computeMaqamDegreeIndices() const
{
    std::set<int> result;
    for (const auto& name : processor.getCurrentDegreeNames())
    {
        auto it = paoNameMap.find (name);
        if (it != paoNameMap.end())
            result.insert (it->second);
    }
    return result;
}

std::map<int, juce::String> TanghimNativeEditor::buildDegreePaoNameMap() const
{
    std::map<int, juce::String> result;
    for (const auto& name : processor.getCurrentDegreeNames())
    {
        auto it = paoNameMap.find (name);
        if (it != paoNameMap.end() && result.find (it->second) == result.end())
            result[it->second] = name;
    }
    return result;
}

std::optional<TanghimNativeEditor::TonicInfo> TanghimNativeEditor::findTonicMidi (
    const juce::String& tonicDisplay) const
{
    for (const auto& [ci, octaveMap] : noteNameMap)
    {
        for (const auto& [octave, displayName] : octaveMap)
        {
            if (displayName == tonicDisplay)
                return TonicInfo { (octave + 1) * 12 + ci, ci };
        }
    }
    return std::nullopt;
}

void TanghimNativeEditor::computeModifiedSlots()
{
    modifiedSlots.clear();
    isMaqamModified = false;

    if (processor.getCurrentMaqamId().isEmpty()) return;

    const auto& state = processor.getActiveTuningState();

    for (int ci = 0; ci < 12; ++ci)
    {
        const auto& slot = state.slots[(size_t) ci];
        auto paoIt = degreePaoNameMap.find (ci);

        if (paoIt != degreePaoNameMap.end())
        {
            // Degree slot: compare cents against expected variant
            for (const auto& v : slot.variants)
            {
                if (v.noteName == paoIt->second)
                {
                    if (std::abs (slot.centsOffset - v.midiCentsDeviation) > 0.01)
                    {
                        modifiedSlots.insert (ci);
                        isMaqamModified = true;
                    }
                    break;
                }
            }
        }
        else
        {
            // Non-degree: compare against default (closest to 0 cents)
            double minAbs = 1e9, defaultCents = 0.0;
            for (const auto& v : slot.variants)
            {
                if (std::abs (v.midiCentsDeviation) < minAbs)
                {
                    minAbs = std::abs (v.midiCentsDeviation);
                    defaultCents = v.midiCentsDeviation;
                }
            }
            if (std::abs (slot.centsOffset - defaultCents) > 0.01)
                modifiedSlots.insert (ci);
        }
    }
}

// ── Layout ────────────────────────────────────────────────────────────────────
//
// Reference layout (from tanghim-ui.png):
//
// ┌─────────────────────────────────────────────────────────────────┐
// │ Top Bar (~180px)                                                │
// │  LEFT (60%)                      │  RIGHT (40%)                 │
// │  [Load] [Save] [Settings]        │  [Ref Freq] 110.00 Hz +0.0¢ │
// │  [System dropdown          ▼]    │  [Osc] [Hept]               │
// │  ['ushayrān ▼] [maqām rāst  ▼]  │  [MTS-ESP] [MPE] [Mono PB]  │
// │  ┌─────────┐ ┌──────────┐        │                             │
// │  │Preset 1 │ │Preset 2  │ ...    │                             │
// │  │Preset 5 │ │Preset 6  │ ...    │                             │
// │  └─────────┘ └──────────┘        │                             │
// ├──────────────────────────────────────────────────────────────────┤
// │ Range: [═══════════●══════] C3–B3                               │
// ├──────────────────────────────────────────────────────────────────┤
// │ NoteSliderBank (flex: 1)                                        │
// ├──────────────────────────────────────────────────────────────────┤
// │ Status Bar (26px)                                               │
// └──────────────────────────────────────────────────────────────────┘

void TanghimNativeEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg);

    // Top bar background — height driven by resized() calculation
    // menu(22) + 4 + system/note(52) + 4 + maqam(24) = 106 inner + 16 padding = 122
    const int topBarH = 122;
    auto topBarArea = getLocalBounds().removeFromTop (topBarH);
    g.setColour (Theme::surface);
    g.fillRect (topBarArea);
    g.setColour (Theme::border);
    g.drawHorizontalLine (topBarArea.getBottom() - 1, 0.0f, (float) getWidth());

    // Preset bar background — 2×52 + 12 padding = 116
    const int presetBarH = 116;
    auto presetArea = getLocalBounds().withY (topBarH).withHeight (presetBarH);
    g.setColour (Theme::surface);
    g.fillRect (presetArea);
    g.setColour (Theme::border);
    g.drawHorizontalLine (presetArea.getBottom() - 1, 0.0f, (float) getWidth());

    // Range scroller background
    const int rangeH = 28;
    auto rangeArea = getLocalBounds().withY (topBarH + presetBarH).withHeight (rangeH);
    g.setColour (Theme::surface);
    g.fillRect (rangeArea);
    g.setColour (Theme::border);
    g.drawHorizontalLine (rangeArea.getBottom() - 1, 0.0f, (float) getWidth());

    // Slider bank background — CSS: background: var(--surface), border-bottom: 1px solid border
    {
        const int sliderBankY = topBarH + presetBarH + rangeH;
        const int sliderBankH = getHeight() - sliderBankY - Theme::kStatusBarHeight;
        auto sliderBankArea = juce::Rectangle<int> (0, sliderBankY, getWidth(), sliderBankH);
        g.setColour (Theme::surface);
        g.fillRect (sliderBankArea);
        g.setColour (Theme::border);
        g.drawHorizontalLine (sliderBankArea.getBottom() - 1, 0.0f, (float) getWidth());
    }

    // Status bar background
    auto statusBarBounds = getLocalBounds().removeFromBottom (Theme::kStatusBarHeight);
    g.setColour (juce::Colour (0xff16162b));
    g.fillRect (statusBarBounds);
    g.setColour (juce::Colour (0xff2d2d4a));
    g.drawHorizontalLine (statusBarBounds.getY(), 0.0f, (float) getWidth());

    // Version + timestamp
    g.setColour (juce::Colour (0xff808099));
    g.setFont (11.0f);
    g.drawText ("v" + juce::String (PLUGIN_VERSION) + " (" + juce::String (BUILD_TIMESTAMP) + ")",
                statusBarBounds.withTrimmedLeft (16).withWidth (200),
                juce::Justification::centredLeft);
}

void TanghimNativeEditor::resized()
{
    auto area = getLocalBounds();

    // ══════════════════════════════════════════════════════════════════════
    // Top bar: padding 8px 16px, flex row, gap 12px
    //   Left (flex 1, column, gap 4px): menu-bar, TuningSystemSelector, MaqamSelector
    //   Right (column, gap 4px, children flex 1): ReferenceFreqControl, OutputBadges
    // ══════════════════════════════════════════════════════════════════════
    {
        auto topBar = area.removeFromTop (122);  // 8+106+8
        auto inner = topBar.reduced (16, 8);     // padding: 8px 16px

        // Split left/right with 12px gap (CSS: gap: 12px)
        // CSS: .top-bar-right auto-sizes but stretches proportionally
        // Right column is ~38% of available width (min 260px for content fit)
        const int rightW = std::max (260, (int) (inner.getWidth() * 0.38));
        auto right = inner.removeFromRight (rightW);
        inner.removeFromRight (12);
        auto left = inner;

        // ── Left column (gap: 4px) ──────────────────────────────────
        // Menu bar: flex row, items ~22px tall
        auto menuRow = left.removeFromTop (22);
        {
            const int btnW = 56;
            const int btnGap = 6;  // CSS: gap: 6px
            loadButton.setBounds (menuRow.removeFromLeft (btnW));
            menuRow.removeFromLeft (btnGap);
            saveButton.setBounds (menuRow.removeFromLeft (btnW));
            menuRow.removeFromLeft (btnGap);
            settingsButton.setBounds (menuRow.removeFromLeft (btnW + 10));
        }
        left.removeFromTop (4);  // gap

        // TuningSystemSelector: 2 rows × 24px + 4px gap = 52px
        tuningSystemSelector.setBounds (left.removeFromTop (52));
        left.removeFromTop (4);  // gap

        // MaqamSelector: 1 row × 24px
        maqamSelector.setBounds (left.removeFromTop (24));

        // ── Right column (gap: 4px, children flex: 1) ──────────────
        const int rightInnerH = right.getHeight();
        const int halfH = (rightInnerH - 4) / 2;

        // ReferenceFreqControl: flex 1
        referenceFreqControl.setBounds (right.removeFromTop (halfH));
        right.removeFromTop (4);  // gap

        // OutputBadges: flex 1
        outputBadges.setBounds (right);
    }

    // ══════════════════════════════════════════════════════════════════════
    // Preset bar: grid 4 columns, gap 4px 6px, padding 6px 16px
    // Each preset button: height 52px → total = 6 + 52 + 4 + 52 + 6 = 120
    // (using 116 to save a few pixels)
    // ══════════════════════════════════════════════════════════════════════
    presetBar.setBounds (area.removeFromTop (116).reduced (16, 6));

    // ══════════════════════════════════════════════════════════════════════
    // RangeScroller: padding 4px 16px → ~28px total
    // ══════════════════════════════════════════════════════════════════════
    rangeScroller.setBounds (area.removeFromTop (28));

    // ── Status bar (26px from bottom) ────────────────────────────────────
    auto statusArea = area.removeFromBottom (Theme::kStatusBarHeight);
    {
        const int btnHeight = 18;
        const int yPos = statusArea.getY() + (Theme::kStatusBarHeight - btnHeight) / 2;
        int rightEdge = getWidth() - 16;

        const int updatesBtnW = 70;
        updatesButton.setBounds (rightEdge - updatesBtnW, yPos, updatesBtnW, btnHeight);
        rightEdge -= updatesBtnW + 4;

        const int clearCacheBtnW = 80;
        clearCacheButton.setBounds (rightEdge - clearCacheBtnW, yPos, clearCacheBtnW, btnHeight);
        rightEdge -= clearCacheBtnW + 8;

        const int midiBtnW = 42;
        midiDragButton.setBounds (rightEdge - midiBtnW, yPos, midiBtnW, btnHeight);
        rightEdge -= midiBtnW + 12;

        const int chW = 50;
        midiChannelSelector.setBounds (rightEdge - chW, yPos, chW, btnHeight);
        rightEdge -= chW + 4;

        const int devW = 120;
        midiDeviceSelector.setBounds (rightEdge - devW, yPos, devW, btnHeight);
        rightEdge -= devW + 4;

        const int prefW = 130;
        midiPresetLabel.setBounds (rightEdge - prefW, yPos, prefW, btnHeight);
    }

    // ── Slider bank (remaining space) ────────────────────────────────────
    noteSliderBank.setBounds (area);

    // Update range scroller with visible count
    rangeScroller.setVisibleCount (noteSliderBank.getVisibleSliderCount());

    // Re-center viewport on resize (wide ≥24: start at yegāh)
    const int vc = noteSliderBank.getVisibleSliderCount();
    if (vc >= 24)
    {
        int yegahMidi = -1;
        const auto& allPCs = processor.getCurrentPitchClasses();
        for (const auto& pc : allPCs)
        {
            if (pc.noteName == "yegah") { yegahMidi = pc.midiNoteNumber; break; }
        }
        if (yegahMidi < 0)
        {
            for (const auto& pc : allPCs)
            {
                if (pc.pitchClassIndex == 0 && pc.octave == 1) { yegahMidi = pc.midiNoteNumber; break; }
            }
        }
        if (yegahMidi < 0) yegahMidi = 48;

        // Yegāh at the left edge of the viewport
        double startMidi = juce::jlimit (0.0, 128.0 - vc, (double) yegahMidi);
        processor.setStartMidi (startMidi);
        noteSliderBank.setScrollPosition (startMidi);
        rangeScroller.setStartMidi (startMidi);
    }
}

// ── Timer callback ────────────────────────────────────────────────────────────

void TanghimNativeEditor::timerCallback()
{
    // ── MIDI-triggered preset (~30Hz) — same path as mouse click ────────
    {
        const int presetIdx = processor.consumePendingMidiPreset();
        if (presetIdx >= 0 && presetIdx < 16)
        {
            const auto& preset = processor.getPresets()[(size_t) presetIdx];
            if (preset.isAssigned)
            {
                if (preset.tuningSystemId.isEmpty())
                {
                    // Unmodified: apply maqam with fresh system defaults
                    processor.applyMaqam (preset.maqamIdName,
                                           preset.isTransposed ? preset.pitchClassSetIndex : -1);
                    processor.setCurrentActivePresetIdx (presetIdx);
                }
                else
                {
                    // Modified: restore saved slider positions/cents
                    processor.applyPreset (presetIdx);
                }
                syncFullState();
            }
        }
    }

    // ── Ref freq automation dirty flag (~30Hz) ────────────────────────────
    if (processor.refFreqAutomationDirty.exchange (false, std::memory_order_relaxed))
        syncFullState();

    // ── Slot automation dirty flags (~30Hz) ───────────────────────────────
    {
        const uint16_t dirtyMask = processor.slotAutomationDirtyMask.exchange (
            0, std::memory_order_relaxed);
        if (dirtyMask != 0)
        {
            const auto& state = processor.getActiveTuningState();
            for (int i = 0; i < 12; ++i)
            {
                if (dirtyMask & (1u << i))
                    noteSliderBank.updateSlotCents (i, state.slots[(size_t) i].centsOffset);
            }
        }
    }

    // ── MIDI activity (~30Hz) ─────────────────────────────────────────────
    {
        std::vector<int> ons, offs;
        for (int w = 0; w < 4; ++w)
        {
            uint32_t onBits  = processor.noteOnBits[w].exchange (0, std::memory_order_relaxed);
            uint32_t offBits = processor.noteOffBits[w].exchange (0, std::memory_order_relaxed);
            for (int b = 0; b < 32; ++b)
            {
                int note = w * 32 + b;
                if (onBits  & (1u << b)) ons.push_back (note);
                if (offBits & (1u << b)) offs.push_back (note);
            }
        }
        if (! ons.empty() || ! offs.empty())
            noteSliderBank.updateMidiActivity (ons, offs);
    }

    // ── MIDI drag button + MTS-ESP status (~2Hz) ──────────────────────────
    if (++mtsStatusFrameCounter >= 15)
    {
        mtsStatusFrameCounter = 0;

        // MIDI drag button visibility
        const auto maqamId = processor.getCurrentMaqamId();
        if (maqamId != lastMaqamId)
        {
            lastMaqamId = maqamId;
            midiDragButton.setVisible (maqamId.isNotEmpty());
        }

        syncMtsStatus();

        // MIDI learn target
        presetBar.setMidiLearnTarget (processor.getMidiLearnTarget());

        // MIDI preset notes
        std::array<int, 16> notes;
        for (int i = 0; i < 16; ++i)
            notes[(size_t) i] = processor.getMidiPresetNote (i);
        presetBar.setMidiPresetNotes (notes);

        // Event-driven MIDI device list refresh
        if (midiDevicesChanged.exchange (false, std::memory_order_relaxed))
        {
            processor.recheckMidiPresetDevice();
            populateMidiDeviceList();
            const auto deviceName = processor.getMidiPresetDevice();
            if (deviceName.isNotEmpty() && ! processor.isMidiPresetDeviceOpen())
                processor.setMidiPresetDevice (deviceName);
        }

        // Slow poll (~5 seconds)
        if (++slowPollCounter >= 10)
        {
            slowPollCounter = 0;
            if (++staleCleanupCounter >= 12)
            {
                staleCleanupCounter = 0;
                ReceiverRegistry::cleanStale (10.0);
            }
        }
    }
}

// ── Status bar setup ──────────────────────────────────────────────────────────

void TanghimNativeEditor::setupStatusBar()
{
    const auto bgColor     = Theme::surface;
    const auto borderColor = Theme::border;
    const auto textColor   = Theme::accent;
    const auto mutedColor  = juce::Colour (0xff808099);

    // MIDI drag button
    addAndMakeVisible (midiDragButton);
    midiDragButton.setVisible (false);

    // Updates button
    addAndMakeVisible (updatesButton);
    updatesButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    updatesButton.setColour (juce::TextButton::textColourOffId, mutedColor);
    updatesButton.onClick = [this]
    {
        processor.checkForDataUpdates (
            [] (auto) {}, [] {}, [] (auto) {}
        );
    };

    // Clear cache button
    addAndMakeVisible (clearCacheButton);
    clearCacheButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    clearCacheButton.setColour (juce::TextButton::textColourOffId, mutedColor);
    clearCacheButton.onClick = [this]
    {
        processor.clearCache();
        clearCacheButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x9c\x93 Cleared!"));
        clearCacheButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff4caf50));
        juce::Component::SafePointer<juce::TextButton> safeBtn (&clearCacheButton);
        juce::Timer::callAfterDelay (1500, [safeBtn] {
            if (auto* btn = safeBtn.getComponent())
            {
                btn->setButtonText (juce::CharPointer_UTF8 ("\xc3\x97 Clear Cache"));
                btn->setColour (juce::TextButton::textColourOffId, juce::Colour (0xff808099));
            }
        });
    };

    // MIDI preset controls
    addAndMakeVisible (midiPresetLabel);
    midiPresetLabel.setColour (juce::Label::textColourId, mutedColor);
    midiPresetLabel.setFont (juce::FontOptions (11.0f));

    addAndMakeVisible (midiDeviceSelector);
    midiDeviceSelector.setLookAndFeel (&statusBarLnF);
    midiDeviceSelector.setColour (juce::ComboBox::backgroundColourId, bgColor);
    midiDeviceSelector.setColour (juce::ComboBox::textColourId, textColor);
    midiDeviceSelector.setColour (juce::ComboBox::outlineColourId, borderColor);
    midiDeviceSelector.setColour (juce::ComboBox::arrowColourId, textColor);
    midiDeviceSelector.onChange = [this]
    {
        int selected = midiDeviceSelector.getSelectedId();
        if (selected <= 1)
            processor.setMidiPresetDevice ("");
        else
        {
            auto devices = processor.getAvailableMidiDevices();
            if (selected - 1 < devices.size())
                processor.setMidiPresetDevice (devices[selected - 1]);
        }
        processor.saveSettingsToDisk();
    };
    populateMidiDeviceList();

    addAndMakeVisible (midiChannelSelector);
    midiChannelSelector.setLookAndFeel (&statusBarLnF);
    midiChannelSelector.setColour (juce::ComboBox::backgroundColourId, bgColor);
    midiChannelSelector.setColour (juce::ComboBox::textColourId, textColor);
    midiChannelSelector.setColour (juce::ComboBox::outlineColourId, borderColor);
    midiChannelSelector.setColour (juce::ComboBox::arrowColourId, textColor);
    midiChannelSelector.onChange = [this]
    {
        int channel = midiChannelSelector.getSelectedId() - 1;
        processor.setMidiPresetChannel (channel);
    };
    populateMidiChannelList();
}

void TanghimNativeEditor::populateMidiDeviceList()
{
    midiDeviceSelector.clear (juce::dontSendNotification);
    auto devices = processor.getAvailableMidiDevices();
    for (int i = 0; i < devices.size(); ++i)
        midiDeviceSelector.addItem (devices[i], i + 1);

    auto currentDevice = processor.getMidiPresetDevice();
    if (currentDevice.isEmpty())
        midiDeviceSelector.setSelectedId (1, juce::dontSendNotification);
    else
    {
        int idx = devices.indexOf (currentDevice);
        midiDeviceSelector.setSelectedId (idx >= 0 ? idx + 1 : 1, juce::dontSendNotification);
    }
}

void TanghimNativeEditor::populateMidiChannelList()
{
    midiChannelSelector.clear();
    midiChannelSelector.addItem ("All", 1);
    for (int ch = 1; ch <= 16; ++ch)
        midiChannelSelector.addItem (juce::String (ch), ch + 1);
    midiChannelSelector.setSelectedId (processor.getMidiPresetChannel() + 1);
}

// ══════════════════════════════════════════════════════════════════════════════
// MidiDragButton (same logic as original)
// ══════════════════════════════════════════════════════════════════════════════

void MidiDragButton::mouseDown (const juce::MouseEvent&)
{
    if (tempMidiFile.existsAsFile())
        tempMidiFile.deleteFile();
    dragStarted = false;
    prepareMidiFile();
}

void MidiDragButton::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && tempMidiFile.existsAsFile() && e.getDistanceFromDragStart() > 4)
    {
        dragStarted = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles (
            { tempMidiFile.getFullPathName() }, true, this, nullptr);
    }
}

void MidiDragButton::prepareMidiFile()
{
    tempMidiFile = juce::File();
    const juce::String maqamId = processor.getCurrentMaqamId();
    if (maqamId.isEmpty()) return;

    const auto& degreeNames = processor.getCurrentDegreeNames();
    if (degreeNames.empty()) return;

    std::map<juce::String, int> paoMap;
    for (const auto& pc : processor.getCurrentPitchClasses())
    {
        if (pc.noteName.isEmpty()) continue;
        int ci = chromaticIndexForIpnRef (pc.ipnReference);
        if (ci >= 0) paoMap[pc.noteName] = ci;
    }

    auto tonicIt = paoMap.find (degreeNames[0]);
    if (tonicIt == paoMap.end()) return;
    int tonicCI = tonicIt->second;

    int tonicMidi = -1;
    for (const auto& pc : processor.getCurrentPitchClasses())
    {
        if (pc.noteName == degreeNames[0] && pc.midiNoteNumber >= 48 && pc.midiNoteNumber < 60)
        { tonicMidi = pc.midiNoteNumber; break; }
    }
    if (tonicMidi < 0)
    {
        for (const auto& pc : processor.getCurrentPitchClasses())
            if (pc.noteName == degreeNames[0]) { tonicMidi = pc.midiNoteNumber; break; }
    }
    if (tonicMidi < 0) return;

    std::vector<int> midiNotes;
    for (const auto& name : degreeNames)
    {
        auto it = paoMap.find (name);
        if (it == paoMap.end()) continue;
        int interval = it->second - tonicCI;
        if (interval < 0) interval += 12;
        int note = tonicMidi + interval;
        if (note >= 0 && note <= 127) midiNotes.push_back (note);
    }
    if (midiNotes.empty()) return;

    MidiFileGenerator::MaqamInfo info;
    info.maqamDisplay = processor.getCurrentMaqamDisplay();
    info.tonicPaoDisplay = processor.getCurrentTonicDisplay();
    info.tonicIpn = processor.getCurrentTonicEnglish();
    info.tonicSolfege = processor.getCurrentTonicSolfege();
    info.midiNotes = midiNotes;

    if (info.maqamDisplay.isEmpty()) info.maqamDisplay = maqamId;
    if (info.tonicPaoDisplay.isEmpty()) info.tonicPaoDisplay = degreeNames[0];

    auto midiData = MidiFileGenerator::generate (info);
    auto filename = MidiFileGenerator::buildFilename (info);

    auto tempDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("Tanghim").getChildFile ("midi-export");
    tempDir.createDirectory();
    tempMidiFile = tempDir.getChildFile (filename);
    tempMidiFile.replaceWithData (midiData.data(), midiData.size());
}
