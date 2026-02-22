#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "api/ApiResponseParser.h"
#include <cmath>
#include <limits>
#include <map>
#include <set>

// ── APVTS parameter layout ────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout ArabicMaqamTunerProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 12 slot params: slot_0 through slot_11 (±150 cents range to match UI)
    for (int i = 0; i < kNumSlotParams; ++i)
    {
        auto id = juce::ParameterID ("slot_" + juce::String (i), 1);
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            id, "Slot " + juce::String (i),
            juce::NormalisableRange<float> (-150.0f, 150.0f, 0.01f),
            0.0f));
    }

    // Reference frequency offset (±700 cents = a perfect fifth each direction)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("ref_freq", 1), "Ref Freq",
        juce::NormalisableRange<float> (-700.0f, 700.0f, 0.01f),
        0.0f));

    // Preset param: "None" + presets 1-16
    juce::StringArray presetChoices;
    presetChoices.add ("None");
    for (int i = 1; i <= 16; ++i)
        presetChoices.add (juce::String (i));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID ("preset", 1), "Preset",
        presetChoices, 0));

    return { params.begin(), params.end() };
}

ArabicMaqamTunerProcessor::ArabicMaqamTunerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      updateChecker (apiClient, dataCache, std::weak_ptr<std::atomic<bool>> (alive))
{
    // Cache parameter pointers for quick access
    for (int i = 0; i < kNumSlotParams; ++i)
        slotParams[i] = dynamic_cast<juce::AudioParameterFloat*> (
            apvts.getParameter ("slot_" + juce::String (i)));

    presetParam = dynamic_cast<juce::AudioParameterChoice*> (
        apvts.getParameter ("preset"));

    refFreqParam = dynamic_cast<juce::AudioParameterFloat*> (
        apvts.getParameter ("ref_freq"));

    // Initialize MIDI preset note mappings to -1 (unmapped)
    for (int i = 0; i < 16; ++i)
        midiPresetNotes[i].store (-1, std::memory_order_relaxed);

    // Register as listener for all params
    for (int i = 0; i < kNumSlotParams; ++i)
        apvts.addParameterListener ("slot_" + juce::String (i), this);
    apvts.addParameterListener ("preset", this);
    apvts.addParameterListener ("ref_freq", this);
    dataCache.loadFromDisk();
    loadPresetsFromDisk();
    loadSettingsFromDisk();

    // Fetch tuning systems list on startup (uses cache if available)
    if (dataCache.hasTuningSystemsList())
    {
        if (onTuningSystemsLoaded) onTuningSystemsLoaded();
    }
    else
    {
        std::weak_ptr<std::atomic<bool>> weak (alive);
        apiClient.fetchTuningSystems (
            [this, weak] (std::vector<TuningSystem> systems)
            {
                if (! isAlive (weak)) return;
                dataCache.setTuningSystemsList (std::move (systems));
                if (onTuningSystemsLoaded) onTuningSystemsLoaded();
            },
            [this, weak] (juce::String err)
            {
                if (! isAlive (weak)) return;
                if (onStatusMessage) onStatusMessage ("Network error: " + err);
            });
    }
}

ArabicMaqamTunerProcessor::~ArabicMaqamTunerProcessor()
{
    // Stop direct MIDI input if active
    if (midiPresetInput)
    {
        midiPresetInput->stop();
        midiPresetInput.reset();
    }

    // Remove APVTS listeners
    for (int i = 0; i < kNumSlotParams; ++i)
        apvts.removeParameterListener ("slot_" + juce::String (i), this);
    apvts.removeParameterListener ("preset", this);
    apvts.removeParameterListener ("ref_freq", this);

    alive->store (false, std::memory_order_release);
    apiClient.cancelPending();
}

// ── AudioProcessor interface ──────────────────────────────────────────────────

void ArabicMaqamTunerProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/) {}
void ArabicMaqamTunerProcessor::releaseResources() {}

void ArabicMaqamTunerProcessor::processBlock (juce::AudioBuffer<float>& audio,
                                               juce::MidiBuffer& midi)
{
    audio.clear();

    // Track Note On / Note Off activity for UI feedback (lock-free, per-note)
    // Preset triggering is handled via direct MIDI device input (handleIncomingMidiMessage)
    for (const auto metadata : midi)
    {
        const auto msg  = metadata.getMessage();
        const auto note = msg.getNoteNumber();
        const auto word = note >> 5;                        // 0-3
        const auto bit  = uint32_t (1u << (note & 31));
        if (msg.isNoteOn())
            noteOnBits[word].fetch_or (bit, std::memory_order_relaxed);
        else if (msg.isNoteOff())
            noteOffBits[word].fetch_or (bit, std::memory_order_relaxed);
    }

    // MIDI passes through unchanged — tuning is applied via MTS-ESP shared memory.
    // Pitch bend output (MPE / mono) is handled exclusively by the Receiver plugin.
}

void ArabicMaqamTunerProcessor::processBlock (juce::AudioBuffer<double>& audio,
                                               juce::MidiBuffer& midi)
{
    audio.clear();

    for (const auto metadata : midi)
    {
        const auto msg  = metadata.getMessage();
        const auto note = msg.getNoteNumber();
        const auto word = note >> 5;
        const auto bit  = uint32_t (1u << (note & 31));
        if (msg.isNoteOn())
            noteOnBits[word].fetch_or (bit, std::memory_order_relaxed);
        else if (msg.isNoteOff())
            noteOffBits[word].fetch_or (bit, std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* ArabicMaqamTunerProcessor::createEditor()
{
    return new ArabicMaqamTunerEditor (*this);
}

// ── State persistence ─────────────────────────────────────────────────────────

void ArabicMaqamTunerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = juce::ValueTree ("ArabicMaqamTunerState");
    state.setProperty ("tuningSystemId",   currentSystemId,     nullptr);
    state.setProperty ("startingNote",     currentStartingNote, nullptr);
    // Slider positions + cents offsets
    auto slidersNode = juce::ValueTree ("SliderPositions");
    for (int i = 0; i < 12; ++i)
    {
        slidersNode.setProperty ("s" + juce::String (i),
                                 activeTuningState.slots[(size_t) i].selectedIndex,
                                 nullptr);
        slidersNode.setProperty ("c" + juce::String (i),
                                 activeTuningState.slots[(size_t) i].centsOffset,
                                 nullptr);
    }
    state.addChild (slidersNode, -1, nullptr);

    // Presets
    auto presetsNode = juce::ValueTree ("Presets");
    for (int i = 0; i < 16; ++i)
    {
        auto n = juce::ValueTree ("P" + juce::String (i));
        const auto& p = presets[(size_t) i];
        n.setProperty ("assigned",        p.isAssigned,       nullptr);
        n.setProperty ("maqamId",         p.maqamIdName,      nullptr);
        n.setProperty ("maqamDisplay",    p.maqamDisplayName, nullptr);
        n.setProperty ("baseMaqamId",     p.baseMaqamIdName,  nullptr);
        n.setProperty ("isTransposed",    p.isTransposed,     nullptr);
        n.setProperty ("tonicNote",       p.tonicNoteName,    nullptr);
        n.setProperty ("tonicIpn",        p.tonicIpnRef,      nullptr);
        n.setProperty ("tonicSolfege",    p.tonicSolfege,     nullptr);
        n.setProperty ("setIdx",          p.pitchClassSetIndex, nullptr);
        for (int j = 0; j < 12; ++j)
            n.setProperty ("sp" + juce::String (j), p.sliderPositions[(size_t) j], nullptr);
        for (int j = 0; j < 12; ++j)
            n.setProperty ("co" + juce::String (j), p.centsOffsets[(size_t) j], nullptr);

        // Degree names (for compatibility checking across tuning systems)
        juce::String degStr;
        for (size_t d = 0; d < p.degreeNames.size(); ++d)
        {
            if (d > 0) degStr += "|";
            degStr += p.degreeNames[d];
        }
        n.setProperty ("degreeNames", degStr, nullptr);

        // Tuning system (for modified presets)
        n.setProperty ("tuningSystemId", p.tuningSystemId, nullptr);
        n.setProperty ("startingNote",   p.startingNote,   nullptr);

        presetsNode.addChild (n, -1, nullptr);
    }
    state.addChild (presetsNode, -1, nullptr);

    // Per-note overrides (sparse: only save non-default entries)
    auto perNoteNode = juce::ValueTree ("PerNoteOverrides");
    for (int i = 0; i < 128; ++i)
    {
        const int ov = activeTuningState.perNoteVariantOverrides[(size_t) i];
        if (ov >= 0)
            perNoteNode.setProperty ("n" + juce::String (i), ov, nullptr);
    }
    state.addChild (perNoteNode, -1, nullptr);

    // Current maqam (for MTS-ESP scale name)
    state.setProperty ("maqamDisplay", currentMaqamDisplay, nullptr);
    state.setProperty ("tonicDisplay", currentTonicDisplay, nullptr);
    state.setProperty ("tonicEnglish", currentTonicEnglish, nullptr);
    state.setProperty ("tonicSolfege", currentTonicSolfege, nullptr);

    // Maqam selection state (for session recall)
    state.setProperty ("selectedMaqamId",    currentMaqamId,           nullptr);
    state.setProperty ("transpositionIndex", currentTranspositionIdx,  nullptr);
    state.setProperty ("activePresetIndex",  currentActivePresetIdx,   nullptr);
    state.setProperty ("startMidi",          currentStartMidi,         nullptr);

    // Degree names (pipe-delimited)
    juce::String degStr;
    for (size_t d = 0; d < currentDegreeNames.size(); ++d)
    {
        if (d > 0) degStr += "|";
        degStr += currentDegreeNames[d];
    }
    state.setProperty ("degreeNames", degStr, nullptr);

    // MIDI preset note mappings, channel, and device
    juce::String midiNotesStr;
    for (int i = 0; i < 16; ++i)
    {
        if (i > 0) midiNotesStr += ",";
        midiNotesStr += juce::String (midiPresetNotes[(size_t) i].load());
    }
    state.setProperty ("midiPresetNotes", midiNotesStr, nullptr);
    state.setProperty ("midiPresetChannel", midiPresetChannel.load(), nullptr);
    state.setProperty ("midiPresetDevice", midiPresetDeviceName, nullptr);

    // Tonic chromatic index for tonic-relative slot mapping
    state.setProperty ("tonicChromatic", currentTonicChromatic, nullptr);

    // Reference frequency offset (global concert pitch)
    state.setProperty ("referenceCentsOffset", referenceCentsOffset, nullptr);

    // Include APVTS state as a child
    state.addChild (apvts.copyState(), -1, nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, dest);
}

void ArabicMaqamTunerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;

    sessionRecallInProgress = true;

    // Restore presets immediately (they're just data)
    auto presetsNode = state.getChildWithName ("Presets");
    for (int i = 0; i < 16; ++i)
    {
        auto n = presetsNode.getChild (i);
        auto& p = presets[(size_t) i];
        p.isAssigned         = (bool) n.getProperty ("assigned", false);
        p.maqamIdName        = n.getProperty ("maqamId").toString();
        p.maqamDisplayName   = n.getProperty ("maqamDisplay").toString();
        p.baseMaqamIdName    = n.getProperty ("baseMaqamId").toString();
        p.isTransposed       = (bool) n.getProperty ("isTransposed", false);
        p.tonicNoteName      = n.getProperty ("tonicNote").toString();
        p.tonicIpnRef        = n.getProperty ("tonicIpn").toString();
        p.tonicSolfege       = n.getProperty ("tonicSolfege").toString();
        p.pitchClassSetIndex = (int) n.getProperty ("setIdx", -1);
        for (int j = 0; j < 12; ++j)
            p.sliderPositions[(size_t) j] = (int) n.getProperty ("sp" + juce::String (j), 0);
        for (int j = 0; j < 12; ++j)
            p.centsOffsets[(size_t) j] = (double) n.getProperty ("co" + juce::String (j), 0.0);

        // Restore degree names
        const juce::String degStr = n.getProperty ("degreeNames").toString();
        p.degreeNames.clear();
        if (degStr.isNotEmpty())
        {
            juce::StringArray parts;
            parts.addTokens (degStr, "|", "");
            for (const auto& s : parts)
                if (s.isNotEmpty()) p.degreeNames.push_back (s);
        }

        // Restore tuning system (for modified presets)
        p.tuningSystemId = n.getProperty ("tuningSystemId").toString();
        p.startingNote   = n.getProperty ("startingNote").toString();
    }

    // Restore maqam display info (for MTS-ESP scale name)
    currentMaqamDisplay = state.getProperty ("maqamDisplay").toString();
    currentTonicDisplay = state.getProperty ("tonicDisplay").toString();
    currentTonicEnglish = state.getProperty ("tonicEnglish").toString();
    currentTonicSolfege = state.getProperty ("tonicSolfege").toString();

    // Restore maqam selection state (capture for use in completion lambda,
    // since loadTuningSystem() clears these)
    const juce::String savedMaqamId    = state.getProperty ("selectedMaqamId").toString();
    const int savedTransIdx            = (int) state.getProperty ("transpositionIndex", -1);
    const int savedPresetIdx           = (int) state.getProperty ("activePresetIndex",  -1);
    const double savedStartMidi        = (double) state.getProperty ("startMidi", 48.0);

    std::vector<juce::String> savedDegreeNames;
    {
        const juce::String ds = state.getProperty ("degreeNames").toString();
        if (ds.isNotEmpty())
        {
            juce::StringArray parts;
            parts.addTokens (ds, "|", "");
            for (const auto& s : parts)
                if (s.isNotEmpty()) savedDegreeNames.push_back (s);
        }
    }

    // Restore tuning system — async (may need API fetch)
    const juce::String sysId    = state.getProperty ("tuningSystemId").toString();
    const juce::String startNote = state.getProperty ("startingNote").toString();

    // Restore APVTS state if present
    auto apvtsChild = state.getChildWithName (apvts.state.getType());
    if (apvtsChild.isValid())
        apvts.replaceState (apvtsChild);

    // Restore MIDI preset note mappings, channel, and device
    auto midiNotesProp = state.getProperty ("midiPresetNotes", juce::var());
    if (! midiNotesProp.isVoid())
    {
        juce::StringArray notes;
        notes.addTokens (midiNotesProp.toString(), ",", "");
        for (int i = 0; i < juce::jmin (16, notes.size()); ++i)
            midiPresetNotes[(size_t) i].store (notes[i].getIntValue(), std::memory_order_relaxed);
    }

    auto midiChanProp = state.getProperty ("midiPresetChannel", juce::var());
    if (! midiChanProp.isVoid())
        midiPresetChannel.store ((int) midiChanProp, std::memory_order_relaxed);

    auto midiDeviceProp = state.getProperty ("midiPresetDevice", juce::var());
    if (! midiDeviceProp.isVoid())
        setMidiPresetDevice (midiDeviceProp.toString());

    // Restore tonic chromatic index
    auto tonicChromProp = state.getProperty ("tonicChromatic", juce::var());
    if (! tonicChromProp.isVoid())
        currentTonicChromatic = juce::jlimit (0, 11, (int) tonicChromProp);

    // Restore reference frequency offset
    auto refCentsProp = state.getProperty ("referenceCentsOffset", juce::var());
    if (! refCentsProp.isVoid())
        referenceCentsOffset = juce::jlimit (-700.0, 700.0, (double) refCentsProp);

    auto slidersNode = state.getChildWithName ("SliderPositions");
    std::array<int, 12> savedPositions;
    std::array<double, 12> savedCentsOffsets;
    savedCentsOffsets.fill (std::numeric_limits<double>::quiet_NaN());
    for (int i = 0; i < 12; ++i)
    {
        savedPositions[(size_t) i] = (int) slidersNode.getProperty ("s" + juce::String (i), 0);
        auto centsProp = slidersNode.getProperty ("c" + juce::String (i), juce::var());
        if (! centsProp.isVoid())
            savedCentsOffsets[(size_t) i] = (double) centsProp;
    }

    // Read per-note overrides (sparse)
    auto perNoteNode = state.getChildWithName ("PerNoteOverrides");
    std::array<int, 128> savedPerNote;
    savedPerNote.fill (-1);
    if (perNoteNode.isValid())
    {
        for (int i = 0; i < 128; ++i)
        {
            auto prop = perNoteNode.getProperty ("n" + juce::String (i), juce::var());
            if (! prop.isVoid())
                savedPerNote[(size_t) i] = (int) prop;
        }
    }

    if (sysId.isNotEmpty() && startNote.isNotEmpty())
    {
        std::weak_ptr<std::atomic<bool>> weak (alive);
        loadTuningSystem (sysId, startNote,
            [this, weak, savedPositions, savedCentsOffsets, savedPerNote,
             savedMaqamId, savedTransIdx, savedPresetIdx, savedStartMidi, savedDegreeNames] ()
        {
            if (! isAlive (weak)) return;

            // Restore slider positions and cents offsets directly (avoid clearing maqam state)
            for (int i = 0; i < 12; ++i)
            {
                auto& slot = activeTuningState.slots[(size_t) i];
                slot.selectedIndex = juce::jlimit (0, slot.variantCount() - 1, savedPositions[(size_t) i]);

                if (! std::isnan (savedCentsOffsets[(size_t) i]))
                    slot.centsOffset = savedCentsOffsets[(size_t) i];
                else if (const auto* v = slot.selectedVariant())
                    slot.centsOffset = v->midiCentsDeviation;

                for (int midi = i; midi < 128; midi += 12)
                    activeTuningState.perNoteVariantOverrides[(size_t) midi] = -1;
            }

            // Restore per-note overrides
            activeTuningState.perNoteVariantOverrides = savedPerNote;

            // Restore maqam selection state
            currentMaqamId           = savedMaqamId;
            currentTranspositionIdx  = savedTransIdx;
            currentActivePresetIdx   = savedPresetIdx;
            currentStartMidi         = savedStartMidi;
            currentDegreeNames       = savedDegreeNames;

            hasRecalledSessionState  = true;
            sessionRecallInProgress  = false;

            // Sync APVTS params after session recall
            syncAllSlotParamsFromState();
            syncPresetParamFromState();

            tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());
            notifyTuningChanged();
        });
    }
    else
    {
        sessionRecallInProgress = false;
    }
}

// ── Tuning control ────────────────────────────────────────────────────────────

void ArabicMaqamTunerProcessor::loadTuningSystem (const juce::String& systemId,
                                                    const juce::String& startingNote,
                                                    std::function<void()> onComplete)
{
    currentSystemId     = systemId;
    currentStartingNote = startingNote;
    currentMaqamList.clear();
    currentMaqamDisplay.clear();
    currentTonicDisplay.clear();
    currentTonicEnglish.clear();
    currentTonicSolfege.clear();

    // Clear maqam selection (system switch invalidates current maqam)
    currentMaqamId.clear();
    currentTranspositionIdx = -1;
    currentActivePresetIdx  = -1;
    currentDegreeNames.clear();
    currentDegreeIpnRefs.fill ({});
    currentDegreeSolfegeRefs.fill ({});
    currentTranspositionIdMap.clear();

    // Reset reference frequency offset (starting note change = new reference point)
    referenceCentsOffset = 0.0;
    if (refFreqParam != nullptr)
    {
        updatingParamsFromCode = true;
        refFreqParam->setValueNotifyingHost (refFreqParam->convertTo0to1 (0.0f));
        updatingParamsFromCode = false;
    }

    auto doLoad = [this, systemId, startingNote, onComplete] ()
    {
        const auto& data = dataCache.getData (systemId, startingNote);
        const auto variants = ApiResponseParser::buildVariantsPerSlot (data.pitchClasses);

        // Collect all previously selected PAO names before rebuilding slots
        std::set<juce::String> prevPaoNames;
        for (int i = 0; i < 12; ++i)
        {
            if (const auto* prev = activeTuningState.slots[(size_t) i].selectedVariant())
                if (prev->noteName.isNotEmpty())
                    prevPaoNames.insert (prev->noteName);
        }

        // Rebuild all slots with new variants, default to variant closest to 0 cents
        activeTuningState.clearPerNoteOverrides();
        for (int i = 0; i < 12; ++i)
        {
            auto& slot = activeTuningState.slots[(size_t) i];
            slot.ipnReference  = kChromaticIpnRefs[i];
            slot.variants      = variants[(size_t) i];

            // Select the variant with the smallest absolute cents deviation from 12-EDO
            int bestIdx = 0;
            double bestDist = std::numeric_limits<double>::max();
            for (int v = 0; v < (int) slot.variants.size(); ++v)
            {
                const double dist = std::abs (slot.variants[(size_t) v].midiCentsDeviation);
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestIdx  = v;
                }
            }
            slot.selectedIndex = bestIdx;
        }

        // PAO name matching: for each slot, if any variant's PAO name
        // was previously selected (in any slot), select that variant.
        // This preserves musical intent across tuning systems even when
        // a note moves to a different IPN position.
        for (int i = 0; i < 12; ++i)
        {
            auto& slot = activeTuningState.slots[(size_t) i];
            for (int v = 0; v < (int) slot.variants.size(); ++v)
            {
                if (prevPaoNames.count (slot.variants[(size_t) v].noteName))
                {
                    slot.selectedIndex = v;
                    break;
                }
            }
        }

        // Sync centsOffset from selected variants
        for (int i = 0; i < 12; ++i)
        {
            auto& sl = activeTuningState.slots[(size_t) i];
            if (const auto* v = sl.selectedVariant())
                sl.centsOffset = v->midiCentsDeviation;
        }

        // Find the tonic pitch class (pitchClassIndex==0, octave==1) for reference frequency
        for (const auto& pc : data.pitchClasses)
        {
            if (pc.pitchClassIndex == 0 && pc.octave == 1)
            {
                referenceNoteMidi = pc.midiNoteNumber;
                referenceNoteDisplayName = pc.noteNameDisplay;
                break;
            }
        }

        // Sync APVTS params after tuning system load
        syncAllSlotParamsFromState();
        syncPresetParamFromState();

        tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());
        if (onStatusMessage) onStatusMessage (buildScaleName());
        notifyTuningChanged();
        if (onComplete) onComplete();

        saveSettingsToDisk();

        // Fetch maqam list after sliders are visible (not on the critical path)
        fetchMaqamListIfNeeded();
    };

    if (dataCache.hasData (systemId, startingNote))
    {
        if (dataCache.isInMemory (systemId, startingNote))
        {
            // Data already deserialized — use immediately
            doLoad();
        }
        else
        {
            // Data on disk but not yet deserialized — preload in background
            // to avoid blocking the message thread with JSON parsing
            std::weak_ptr<std::atomic<bool>> weak (alive);
            apiClient.runOnThread ([this, weak, systemId, startingNote, doLoad]
            {
                dataCache.preload (systemId, startingNote);
                juce::MessageManager::callAsync ([weak, doLoad]
                {
                    if (! isAlive (weak)) return;
                    doLoad();
                });
            });
        }
    }
    else
    {
        if (onStatusMessage) onStatusMessage ("Loading " + systemId + "…");
        std::weak_ptr<std::atomic<bool>> weak (alive);
        apiClient.fetchPitchClasses (systemId, startingNote,
            [this, weak, systemId, startingNote, doLoad] (std::vector<PitchClass> pcs)
            {
                if (! isAlive (weak)) return;
                ApiDataCache::TuningData td;
                td.pitchClasses = std::move (pcs);
                td.lastChecked  = juce::Time::getCurrentTime().toISO8601 (true);

                // Store the version from the tuning systems list so update
                // checker can compare versions and skip re-fetches
                for (const auto& ts : dataCache.getTuningSystemsList())
                    if (ts.id == systemId) { td.tuningSystemVersion = ts.version; break; }

                dataCache.storeData (systemId, startingNote, std::move (td));
                doLoad();
            },
            [this, weak] (juce::String err)
            {
                if (! isAlive (weak)) return;
                if (onStatusMessage) onStatusMessage ("Error: " + err);
            });
    }
}

void ArabicMaqamTunerProcessor::setSliderVariant (int chromaticIndex, int variantIndex)
{
    if (chromaticIndex < 0 || chromaticIndex >= 12) return;
    auto& slot = activeTuningState.slots[(size_t) chromaticIndex];
    slot.selectedIndex = juce::jlimit (0, slot.variantCount() - 1, variantIndex);

    // Sync centsOffset to the selected variant's deviation
    if (const auto* v = slot.selectedVariant())
        slot.centsOffset = v->midiCentsDeviation;

    // Clear per-note overrides for this chromatic position (all-octaves reset)
    for (int midi = chromaticIndex; midi < 128; midi += 12)
        activeTuningState.perNoteVariantOverrides[(size_t) midi] = -1;

    // Manual slider change breaks maqam association
    currentMaqamId.clear();
    currentTranspositionIdx = -1;
    currentActivePresetIdx  = -1;
    currentDegreeNames.clear();

    // Sync APVTS params
    syncSlotParamFromState (chromaticIndex);
    syncPresetParamFromState();

    tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());
    notifyTuningChanged();
}

void ArabicMaqamTunerProcessor::setSlotCents (int chromaticIndex, double centsValue)
{
    if (chromaticIndex < 0 || chromaticIndex >= 12) return;
    auto& slot = activeTuningState.slots[(size_t) chromaticIndex];
    slot.centsOffset = juce::jlimit (-200.0, 200.0, centsValue);

    // Update selectedIndex to nearest variant (for display/highlighting)
    int bestIdx = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (int v = 0; v < slot.variantCount(); ++v)
    {
        const double dist = std::abs (slot.variants[(size_t) v].midiCentsDeviation - centsValue);
        if (dist < bestDist) { bestDist = dist; bestIdx = v; }
    }
    slot.selectedIndex = bestIdx;

    // Manual slider change breaks maqam association
    currentMaqamId.clear();
    currentTranspositionIdx = -1;
    currentActivePresetIdx  = -1;
    currentDegreeNames.clear();

    // Sync APVTS param for DAW automation recording
    syncSlotParamFromState (chromaticIndex);

    // Update MTS-ESP immediately (live pitch change) — no WebView push
    tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());
}

void ArabicMaqamTunerProcessor::finalizeSlotCents (int chromaticIndex, double centsValue)
{
    setSlotCents (chromaticIndex, centsValue);
    notifyTuningChanged();
}

void ArabicMaqamTunerProcessor::setNoteVariant (int midiNote, int variantIndex)
{
    if (midiNote < 0 || midiNote >= 128) return;
    const int chromaticIdx = midiNote % 12;
    const auto& slot = activeTuningState.slots[(size_t) chromaticIdx];
    activeTuningState.perNoteVariantOverrides[(size_t) midiNote] =
        juce::jlimit (0, std::max (0, slot.variantCount() - 1), variantIndex);

    // Manual per-note override breaks maqam association
    currentMaqamId.clear();
    currentTranspositionIdx = -1;
    currentActivePresetIdx  = -1;
    currentDegreeNames.clear();

    // Sync preset param only (per-note overrides don't affect slot params)
    syncPresetParamFromState();

    tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());
    notifyTuningChanged();
}

void ArabicMaqamTunerProcessor::applyPreset (int idx)
{
    if (idx < 0 || idx >= 16) return;
    const auto& preset = presets[(size_t) idx];
    if (! preset.isAssigned) return;

    // Apply all slider positions and cents offsets directly (not via setSliderVariant which clears maqam state)
    activeTuningState.clearPerNoteOverrides();
    for (int i = 0; i < 12; ++i)
    {
        auto& slot = activeTuningState.slots[(size_t) i];
        slot.selectedIndex = juce::jlimit (0, slot.variantCount() - 1, preset.sliderPositions[(size_t) i]);
        slot.centsOffset = preset.centsOffsets[(size_t) i];
    }

    // Restore maqam state from preset
    currentActivePresetIdx  = idx;
    currentMaqamId          = preset.maqamIdName;
    currentDegreeNames      = preset.degreeNames;
    currentTranspositionIdx = preset.isTransposed ? preset.pitchClassSetIndex : -1;

    // Set tonic chromatic index from first degree name (for tonic-relative slot mapping)
    if (! preset.degreeNames.empty() && dataCache.hasData (currentSystemId, currentStartingNote))
    {
        const auto& data = dataCache.getData (currentSystemId, currentStartingNote);
        for (const auto& pc : data.pitchClasses)
        {
            if (pc.noteName == preset.degreeNames[0])
            {
                currentTonicChromatic = chromaticIndexForIpnRef (pc.ipnReference);
                break;
            }
        }
    }

    // Update display strings from preset
    currentMaqamDisplay = preset.maqamDisplayName;
    currentTonicDisplay.clear();
    currentTonicEnglish.clear();
    currentTonicSolfege.clear();
    if (preset.tonicNoteName.isNotEmpty() && dataCache.hasData (currentSystemId, currentStartingNote))
    {
        const auto& data = dataCache.getData (currentSystemId, currentStartingNote);
        for (const auto& pc : data.pitchClasses)
        {
            if (pc.noteNameDisplay == preset.tonicNoteName)
            {
                currentTonicDisplay = pc.noteNameDisplay;
                currentTonicEnglish = pc.englishName;
                currentTonicSolfege = pc.solfege;
                break;
            }
        }
    }

    // Sync APVTS params
    syncAllSlotParamsFromState();
    syncPresetParamFromState();

    tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());
    notifyTuningChanged();
}

void ArabicMaqamTunerProcessor::assignPreset (int idx,
                                               const juce::String& maqamId,
                                               const juce::String& maqamDisplay,
                                               const juce::String& baseMaqamId,
                                               bool isTransposed,
                                               const juce::String& tonicNote,
                                               const juce::String& tonicIpn,
                                               const juce::String& tonicSolfege,
                                               int setIdx,
                                               const std::array<int, 12>& positions,
                                               const std::vector<juce::String>& degreeNames,
                                               const std::array<double, 12>& centsOffsets,
                                               const juce::String& tuningSystemId,
                                               const juce::String& startingNote)
{
    if (idx < 0 || idx >= 16) return;
    auto& p              = presets[(size_t) idx];
    p.isAssigned         = true;
    p.maqamIdName        = maqamId;
    p.maqamDisplayName   = maqamDisplay;
    p.baseMaqamIdName    = baseMaqamId;
    p.isTransposed       = isTransposed;
    p.tonicNoteName      = tonicNote;
    p.tonicIpnRef        = tonicIpn;
    p.tonicSolfege       = tonicSolfege;
    p.pitchClassSetIndex = setIdx;
    p.sliderPositions    = positions;
    p.degreeNames        = degreeNames;
    p.centsOffsets       = centsOffsets;
    p.tuningSystemId     = tuningSystemId;
    p.startingNote       = startingNote;

    currentActivePresetIdx = idx;
    savePresetsToDisk();
}

void ArabicMaqamTunerProcessor::applyMaqam (const juce::String& maqamId, int transpositionIndex)
{
    // Find the maqam in the current list
    const MaqamListEntry* found = nullptr;
    for (const auto& mle : currentMaqamList)
    {
        if (mle.maqamId == maqamId)
        {
            found = &mle;
            break;
        }
    }
    if (found == nullptr) return;

    // Track maqam selection state
    currentMaqamId          = maqamId;
    currentTranspositionIdx = transpositionIndex;
    currentActivePresetIdx  = -1; // maqam applied directly, not via preset

    // Store current maqam info for MTS-ESP scale name
    currentMaqamDisplay = found->maqamDisplay;

    // Choose base degrees or a specific transposition
    juce::String tonicId;
    if (transpositionIndex < 0 || transpositionIndex >= (int) found->transpositions.size())
    {
        currentTonicDisplay = found->tonicDisplay;
        tonicId = found->tonicId;

        // Store degree names BEFORE applyMaqamDegrees, since it calls notifyTuningChanged()
        currentDegreeNames.clear();
        for (const auto& name : found->degrees.ascending)
            currentDegreeNames.push_back (name);

        applyMaqamDegrees (found->degrees);
    }
    else
    {
        const auto& t = found->transpositions[(size_t) transpositionIndex];
        currentTonicDisplay = t.tonicDisplay;
        tonicId = t.tonicId;

        // Store degree names BEFORE applyMaqamDegrees, since it calls notifyTuningChanged()
        currentDegreeNames.clear();
        for (const auto& name : t.degrees.ascending)
            currentDegreeNames.push_back (name);

        applyMaqamDegrees (t.degrees);
    }

    // Look up tonic's IPN and solfège from pitch class data
    currentTonicEnglish.clear();
    currentTonicSolfege.clear();
    if (tonicId.isNotEmpty() && dataCache.hasData (currentSystemId, currentStartingNote))
    {
        const auto& data = dataCache.getData (currentSystemId, currentStartingNote);
        for (const auto& pc : data.pitchClasses)
        {
            if (pc.noteName == tonicId)
            {
                currentTonicEnglish = pc.englishName;
                currentTonicSolfege = pc.solfege;
                break;
            }
        }
    }

    // Fetch maqam detail for context-aware IPN labels and solfege (async, updates on arrival)
    currentDegreeIpnRefs.fill ({});
    currentDegreeSolfegeRefs.fill ({});
    fetchAndApplyMaqamDetail();
}

void ArabicMaqamTunerProcessor::applyDegreeIpnRefs (const MaqamDetailResult& detail)
{
    currentDegreeIpnRefs.fill ({});
    currentDegreeSolfegeRefs.fill ({});
    for (const auto& pc : detail.ascendingDegrees)
    {
        if (pc.ipnReference.isEmpty()) continue;
        const int ci = chromaticIndexForIpnRef (pc.ipnReference);
        if (ci >= 0)
        {
            currentDegreeIpnRefs[(size_t) ci] = pc.ipnReference;
            if (pc.solfege.isNotEmpty())
                currentDegreeSolfegeRefs[(size_t) ci] = pc.solfege;
        }
    }
}

void ArabicMaqamTunerProcessor::fetchAndApplyMaqamDetail()
{
    if (currentSystemId.isEmpty() || currentStartingNote.isEmpty()) return;
    if (currentMaqamId.isEmpty()) return;

    // Determine the maqam endpoint ID and transposition ID
    // For transpositions, we need to look up the transposition idName from
    // the base maqam's availableTranspositions mapping.
    juce::String fetchMaqamId = currentMaqamId;
    juce::String transpositionId;

    if (currentTranspositionIdx >= 0)
    {
        // Find the tonic ID for the current transposition
        const MaqamListEntry* found = nullptr;
        for (const auto& mle : currentMaqamList)
        {
            if (mle.maqamId == currentMaqamId)
            {
                found = &mle;
                break;
            }
        }

        if (found != nullptr && currentTranspositionIdx < (int) found->transpositions.size())
        {
            const auto& tonicId = found->transpositions[(size_t) currentTranspositionIdx].tonicId;

            // Look up the transposition idName from the cached base maqam detail
            auto transIt = currentTranspositionIdMap.find (tonicId);
            if (transIt != currentTranspositionIdMap.end())
            {
                transpositionId = transIt->second;
            }
            else
            {
                // We don't have the transposition map yet — fetch the base maqam first
                // to get availableTranspositions, then re-invoke for the transposition
                std::weak_ptr<std::atomic<bool>> weak (alive);
                const auto sysId = currentSystemId;
                const auto startNote = currentStartingNote;
                const auto maqId = currentMaqamId;

                if (dataCache.hasMaqamDetail (sysId, startNote, maqId))
                {
                    const auto& baseDetail = dataCache.getMaqamDetail (sysId, startNote, maqId);
                    currentTranspositionIdMap = baseDetail.transpositionIdMap;
                    auto it2 = currentTranspositionIdMap.find (tonicId);
                    if (it2 != currentTranspositionIdMap.end())
                        transpositionId = it2->second;
                }
                else
                {
                    // Fetch base maqam detail to get the transposition map
                    apiClient.fetchMaqamDetail (maqId, sysId, startNote,
                        [this, weak, sysId, startNote, maqId] (MaqamDetailResult baseDetail)
                        {
                            if (! isAlive (weak)) return;
                            if (currentMaqamId != maqId) return; // user changed maqam

                            dataCache.storeMaqamDetail (sysId, startNote, maqId, baseDetail);
                            currentTranspositionIdMap = baseDetail.transpositionIdMap;

                            // Now re-invoke to fetch the transposition with the map populated
                            fetchAndApplyMaqamDetail();
                        });
                    return;
                }
            }
        }
    }

    // Build the cache key: for transpositions, include the transpositionId
    // so each transposition is cached separately
    juce::String cacheKey = transpositionId.isNotEmpty()
        ? (currentMaqamId + ":" + transpositionId)
        : currentMaqamId;

    // Check cache first
    if (dataCache.hasMaqamDetail (currentSystemId, currentStartingNote, cacheKey))
    {
        const auto& detail = dataCache.getMaqamDetail (currentSystemId, currentStartingNote, cacheKey);
        applyDegreeIpnRefs (detail);
        if (! currentTranspositionIdMap.empty() || currentTranspositionIdx < 0)
            currentTranspositionIdMap = detail.transpositionIdMap;
        notifyTuningChanged();
        return;
    }

    // Fetch from API
    std::weak_ptr<std::atomic<bool>> weak (alive);
    const auto sysId = currentSystemId;
    const auto startNote = currentStartingNote;
    const auto maqId = currentMaqamId;

    apiClient.fetchMaqamDetail (fetchMaqamId, sysId, startNote,
        [this, weak, sysId, startNote, cacheKey, maqId] (MaqamDetailResult detail)
        {
            if (! isAlive (weak)) return;
            if (currentMaqamId != maqId) return; // user changed maqam while fetching

            dataCache.storeMaqamDetail (sysId, startNote, cacheKey, detail);
            applyDegreeIpnRefs (detail);
            if (! detail.transpositionIdMap.empty())
                currentTranspositionIdMap = detail.transpositionIdMap;
            notifyTuningChanged();
        },
        {} /* onError — silently fallback to default IPN labels */,
        transpositionId);
}

void ArabicMaqamTunerProcessor::clearPreset (int idx)
{
    if (idx < 0 || idx >= 16) return;
    presets[(size_t) idx].clear();

    if (currentActivePresetIdx == idx)
        currentActivePresetIdx = -1;

    savePresetsToDisk();
}

// ── Reference frequency control ───────────────────────────────────────────────

double ArabicMaqamTunerProcessor::getReferenceDefaultHz() const
{
    return 440.0 * std::pow (2.0, (referenceNoteMidi - 69) / 12.0);
}

double ArabicMaqamTunerProcessor::getReferenceCurrentHz() const
{
    return getReferenceDefaultHz() * std::pow (2.0, referenceCentsOffset / 1200.0);
}

void ArabicMaqamTunerProcessor::setReferenceCentsOffset (double cents)
{
    referenceCentsOffset = juce::jlimit (-700.0, 700.0, cents);

    // Sync APVTS param
    if (refFreqParam != nullptr)
    {
        updatingParamsFromCode = true;
        refFreqParam->setValueNotifyingHost (
            refFreqParam->convertTo0to1 (static_cast<float> (referenceCentsOffset)));
        updatingParamsFromCode = false;
    }

    tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());
}

void ArabicMaqamTunerProcessor::finalizeReferenceCentsOffset (double cents)
{
    setReferenceCentsOffset (cents);
    notifyTuningChanged();
}

void ArabicMaqamTunerProcessor::beginRefFreqGesture()
{
    if (refFreqParam != nullptr)
        refFreqParam->beginChangeGesture();
}

void ArabicMaqamTunerProcessor::endRefFreqGesture()
{
    if (refFreqParam != nullptr)
        refFreqParam->endChangeGesture();
}

// ── Accessors ─────────────────────────────────────────────────────────────────

const std::vector<TuningSystem>& ArabicMaqamTunerProcessor::getTuningSystems() const
{
    return dataCache.getTuningSystemsList();
}

const std::vector<MaqamListEntry>& ArabicMaqamTunerProcessor::getMaqamList() const
{
    return currentMaqamList;
}

const ActiveTuningState& ArabicMaqamTunerProcessor::getActiveTuningState() const
{
    return activeTuningState;
}

const std::array<MaqamPreset, 16>& ArabicMaqamTunerProcessor::getPresets() const
{
    return presets;
}

juce::String ArabicMaqamTunerProcessor::getCurrentSystemId()  const { return currentSystemId; }
juce::String ArabicMaqamTunerProcessor::getCurrentStartingNote() const { return currentStartingNote; }

const std::vector<PitchClass>& ArabicMaqamTunerProcessor::getCurrentPitchClasses() const
{
    static const std::vector<PitchClass> empty;
    if (currentSystemId.isEmpty() || currentStartingNote.isEmpty())
        return empty;
    if (! dataCache.hasData (currentSystemId, currentStartingNote))
        return empty;
    return dataCache.getData (currentSystemId, currentStartingNote).pitchClasses;
}

bool           ArabicMaqamTunerProcessor::isMtsTransmitter()   const { return tuningEngine.isMtsTransmitter(); }
int            ArabicMaqamTunerProcessor::mtsNumReceivers()   const { return tuningEngine.mtsNumReceivers(); }
ReceiverCounts ArabicMaqamTunerProcessor::getReceiverCounts() const { return ReceiverRegistry::scan(); }

void ArabicMaqamTunerProcessor::checkForDataUpdates (
    std::function<void (std::vector<juce::String>)> onUpdatesFound,
    std::function<void()>                            onNoUpdates,
    std::function<void (juce::String)>               onError)
{
    updateChecker.checkForUpdates (std::move (onUpdatesFound),
                                   std::move (onNoUpdates),
                                   std::move (onError));
}

// ── Private helpers ───────────────────────────────────────────────────────────

void ArabicMaqamTunerProcessor::fetchMaqamListIfNeeded()
{
    if (currentSystemId.isEmpty() || currentStartingNote.isEmpty()) return;

    // Check if cache already has maqam list for the current system
    if (dataCache.hasData (currentSystemId, currentStartingNote))
    {
        const auto& data = dataCache.getData (currentSystemId, currentStartingNote);
        if (! data.maqamList.empty())
        {
            currentMaqamList = data.maqamList;
            DBG ("fetchMaqamListIfNeeded: loaded " + juce::String ((int) currentMaqamList.size()) + " maqamat from cache");
            if (onMaqamListLoaded) onMaqamListLoaded();
            return;
        }
    }

    // Fetch from API
    currentMaqamList.clear();
    DBG ("fetchMaqamListIfNeeded: fetching from API...");

    std::weak_ptr<std::atomic<bool>> weak (alive);
    apiClient.fetchMaqamList (currentSystemId, currentStartingNote,
        [this, weak] (std::vector<MaqamListEntry> entries)
        {
            if (! isAlive (weak)) return;
            DBG ("fetchMaqamListIfNeeded: API returned " + juce::String ((int) entries.size()) + " maqamat");
            currentMaqamList = entries;
            dataCache.updateMaqamList (currentSystemId, currentStartingNote, entries);
            if (onMaqamListLoaded) onMaqamListLoaded();
        },
        [this, weak] (juce::String err)
        {
            if (! isAlive (weak)) return;
            DBG ("fetchMaqamListIfNeeded: API ERROR: " + err);
            if (onStatusMessage) onStatusMessage ("Maqam list: " + err);
        });
}

void ArabicMaqamTunerProcessor::applyMaqamDegrees (const MaqamDegrees& degrees)
{
    if (currentSystemId.isEmpty() || currentStartingNote.isEmpty()) return;
    if (! dataCache.hasData (currentSystemId, currentStartingNote)) return;

    const auto& data = dataCache.getData (currentSystemId, currentStartingNote);

    // Build a lookup from ALL pitch classes (all octaves): PAO noteName → { chromaticIndex, midiCentsDeviation }
    // This covers register-specific names (kirdan, muhayyar, etc.) that exist only outside MIDI 48-59.
    struct PcInfo { int chromaticIdx; double deviation; };
    std::map<juce::String, PcInfo> nameToInfo;

    for (const auto& pc : data.pitchClasses)
    {
        if (pc.noteName.isEmpty()) continue;
        const int ci = chromaticIndexForIpnRef (pc.ipnReference);
        if (ci >= 0 && nameToInfo.count (pc.noteName) == 0)
            nameToInfo[pc.noteName] = { ci, pc.midiCentsDeviation };
    }

    // Reset all slots to the variant closest to 0 cents before applying maqam degrees,
    // so we don't accumulate selections from previously applied maqamat
    // (same logic as loadTuningSystem)
    activeTuningState.clearPerNoteOverrides();
    for (int ci = 0; ci < 12; ++ci)
    {
        auto& sl = activeTuningState.slots[(size_t) ci];

        // Select the variant with the smallest absolute cents deviation from 12-EDO
        int bestIdx = 0;
        double bestDist = std::numeric_limits<double>::max();
        for (int v = 0; v < (int) sl.variants.size(); ++v)
        {
            const double dist = std::abs (sl.variants[(size_t) v].midiCentsDeviation);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestIdx  = v;
            }
        }
        sl.selectedIndex = bestIdx;
        if (const auto* v = sl.selectedVariant())
            sl.centsOffset = v->midiCentsDeviation;
    }

    // Set tonic chromatic index from first ascending degree (for tonic-relative slot mapping)
    if (! degrees.ascending.empty())
    {
        auto tonicIt = nameToInfo.find (degrees.ascending[0]);
        if (tonicIt != nameToInfo.end())
            currentTonicChromatic = tonicIt->second.chromaticIdx;
    }

    // Apply ascending degrees: for each PAO name in the scale,
    // find the matching slider variant by chromatic index and cents deviation.
    // Register-specific names (e.g. "kirdan" at C5) share the same midiCentsDeviation
    // as their base-register equivalents (e.g. "rast" at C3), so we match by deviation.
    for (const auto& noteName : degrees.ascending)
    {
        auto it = nameToInfo.find (noteName);
        if (it == nameToInfo.end()) continue;

        const int ci = it->second.chromaticIdx;
        const double targetDev = it->second.deviation;
        auto& slot = activeTuningState.slots[(size_t) ci];

        // Find the variant with the closest matching cents deviation
        int bestVariant = 0;
        double bestDist = std::numeric_limits<double>::max();
        for (int vi = 0; vi < (int) slot.variants.size(); ++vi)
        {
            const double dist = std::abs (slot.variants[(size_t) vi].midiCentsDeviation - targetDev);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestVariant = vi;
            }
        }
        slot.selectedIndex = bestVariant;
        if (const auto* v = slot.selectedVariant())
            slot.centsOffset = v->midiCentsDeviation;
    }

    // Sync APVTS params
    syncAllSlotParamsFromState();
    syncPresetParamFromState();

    tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());
    notifyTuningChanged();
}

void ArabicMaqamTunerProcessor::rebuildTuningStateFromCache()
{
    if (currentSystemId.isEmpty() || currentStartingNote.isEmpty()) return;
    if (! dataCache.hasData (currentSystemId, currentStartingNote)) return;

    const auto& data = dataCache.getData (currentSystemId, currentStartingNote);
    const auto variants = ApiResponseParser::buildVariantsPerSlot (data.pitchClasses);

    for (int i = 0; i < 12; ++i)
    {
        auto& slot = activeTuningState.slots[(size_t) i];
        const int prevIdx = slot.selectedIndex;
        slot.variants = variants[(size_t) i];
        slot.selectedIndex = juce::jlimit (0, std::max (0, slot.variantCount() - 1), prevIdx);
        if (const auto* v = slot.selectedVariant())
            slot.centsOffset = v->midiCentsDeviation;
    }
}

void ArabicMaqamTunerProcessor::notifyTuningChanged()
{
    if (onTuningStateChanged) onTuningStateChanged();
}

juce::String ArabicMaqamTunerProcessor::buildScaleName() const
{
    if (currentSystemId.isEmpty()) return "Tanghim";

    // Line 1: tuning system short name
    juce::String line1;
    const auto& systems = dataCache.getTuningSystemsList();
    for (const auto& ts : systems)
    {
        if (ts.id == currentSystemId)
        {
            line1 = ts.shortName;
            break;
        }
    }
    if (line1.isEmpty())
        line1 = currentSystemId;

    // Line 2: maqam al-tonic / IPN / solfège (if a maqam is applied)
    if (currentMaqamDisplay.isEmpty())
        return line1;

    juce::String line2 = currentMaqamDisplay;
    if (currentTonicDisplay.isNotEmpty())
        line2 += " al" + juce::String::charToString (0x2011) + currentTonicDisplay;
    if (currentTonicEnglish.isNotEmpty())
        line2 += " / " + currentTonicEnglish;
    if (currentTonicSolfege.isNotEmpty())
        line2 += " / " + currentTonicSolfege;

    return line1 + "\n" + line2;
}

// ── APVTS listener ────────────────────────────────────────────────────────────

void ArabicMaqamTunerProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Skip if we're programmatically updating params to avoid feedback loops
    if (updatingParamsFromCode)
        return;

    // Handle slot parameters (slot_0 through slot_11)
    // Mapping is tonic-relative: slot_0 = tonic, slot_1 = tonic+1, etc.
    if (parameterID.startsWith ("slot_"))
    {
        const int slotIdx = parameterID.substring (5).getIntValue();
        if (slotIdx < 0 || slotIdx >= 12)
            return;

        // Map from tonic-relative slot index to absolute chromatic index
        const int chromaticIdx = slotToChromatic (slotIdx);

        // newValue is cents deviation — find nearest variant and update centsOffset
        auto& slot = activeTuningState.slots[(size_t) chromaticIdx];
        const double centsValue = juce::jlimit (-150.0, 150.0, (double) newValue);

        // Find nearest variant
        int bestIdx = 0;
        double bestDist = std::numeric_limits<double>::max();
        for (int v = 0; v < slot.variantCount(); ++v)
        {
            const double dist = std::abs (slot.variants[(size_t) v].midiCentsDeviation - centsValue);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestIdx = v;
            }
        }
        slot.selectedIndex = bestIdx;
        slot.centsOffset = centsValue;

        // Clear per-note overrides for this chromatic position
        for (int midi = chromaticIdx; midi < 128; midi += 12)
            activeTuningState.perNoteVariantOverrides[(size_t) midi] = -1;

        // NOTE: We intentionally do NOT clear maqam state here.
        // DAW automation should update tuning without breaking maqam association.
        // The explicit methods (setSliderVariant, setSlotCents) handle maqam clearing
        // for direct user interactions. Also, parameterChanged can be called
        // asynchronously from syncAllSlotParamsFromState, after updatingParamsFromCode
        // is reset, which would incorrectly clear maqam state.

        tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());

        // Debug: track automation rate
        static int slotChangeCount = 0;
        static juce::int64 lastLogTime = 0;
        ++slotChangeCount;
        const auto now = juce::Time::currentTimeMillis();
        if (now - lastLogTime > 1000)
        {
            DBG ("Slot automation: " << slotChangeCount << " changes/sec, slot=" << slotIdx
                 << " chromatic=" << chromaticIdx << " cents=" << centsValue);
            slotChangeCount = 0;
            lastLogTime = now;
        }

        // Use lightweight callback for high-rate automation (avoids full state JSON rebuild)
        // Note: emit chromatic index (not slot index) for JS to update the correct slot
        if (onSlotCentsChanged)
            onSlotCentsChanged (chromaticIdx, centsValue);
        else
            notifyTuningChanged();
    }
    // Handle reference frequency parameter
    else if (parameterID == "ref_freq")
    {
        referenceCentsOffset = juce::jlimit (-700.0, 700.0, (double) newValue);
        tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());
        notifyTuningChanged();
    }
    // Handle preset parameter
    else if (parameterID == "preset")
    {
        // newValue is normalized 0-1, use the parameter's getIndex() for actual choice
        if (presetParam == nullptr)
            return;

        const int choiceIdx = presetParam->getIndex(); // 0 = "None", 1-16 = presets 1-16
        const int presetIdx = choiceIdx - 1;           // -1 = "None", 0-15 = presets 1-16

        if (presetIdx >= 0 && presetIdx < 16)
        {
            const auto& preset = presets[(size_t) presetIdx];
            if (! preset.isAssigned)
                return;

            // Check if preset requires a different tuning system (modified preset)
            const bool needsSystemSwitch =
                preset.tuningSystemId.isNotEmpty() &&
                (preset.tuningSystemId != currentSystemId ||
                 preset.startingNote != currentStartingNote);

            if (needsSystemSwitch)
            {
                // Queue the preset application for after the tuning system loads
                const int capturedIdx = presetIdx;
                loadTuningSystem (preset.tuningSystemId, preset.startingNote,
                    [this, capturedIdx] ()
                    {
                        applyPreset (capturedIdx);
                    });
            }
            else
            {
                applyPreset (presetIdx);
            }
        }
        else
        {
            // "None" selected — clear active preset but keep current tuning
            currentActivePresetIdx = -1;
            notifyTuningChanged();
        }
    }
}

// ── APVTS sync helpers ────────────────────────────────────────────────────────

void ArabicMaqamTunerProcessor::syncSlotParamFromState (int chromaticIndex)
{
    if (chromaticIndex < 0 || chromaticIndex >= kNumSlotParams)
        return;

    // Map chromatic index to tonic-relative slot index
    const int slotIdx = chromaticToSlot (chromaticIndex);
    auto* param = slotParams[(size_t) slotIdx];
    if (param == nullptr)
        return;

    const auto& slot = activeTuningState.slots[(size_t) chromaticIndex];
    const float cents = static_cast<float> (juce::jlimit (-150.0, 150.0, slot.centsOffset));

    updatingParamsFromCode = true;
    param->setValueNotifyingHost (param->convertTo0to1 (cents));
    updatingParamsFromCode = false;
}

void ArabicMaqamTunerProcessor::syncAllSlotParamsFromState()
{
    updatingParamsFromCode = true;
    for (int slotIdx = 0; slotIdx < kNumSlotParams; ++slotIdx)
    {
        auto* param = slotParams[(size_t) slotIdx];
        if (param == nullptr)
            continue;

        // Map tonic-relative slot index to chromatic index
        const int chromaticIdx = slotToChromatic (slotIdx);
        const auto& slot = activeTuningState.slots[(size_t) chromaticIdx];
        const float cents = static_cast<float> (juce::jlimit (-150.0, 150.0, slot.centsOffset));
        param->setValueNotifyingHost (param->convertTo0to1 (cents));
    }
    updatingParamsFromCode = false;
}

void ArabicMaqamTunerProcessor::syncPresetParamFromState()
{
    if (presetParam == nullptr)
        return;

    // currentActivePresetIdx is -1 for "None", 0-15 for presets 1-16
    // APVTS choice index: 0 = "None", 1-16 = presets 1-16
    const int choiceIdx = currentActivePresetIdx + 1;

    updatingParamsFromCode = true;
    presetParam->setValueNotifyingHost (
        presetParam->convertTo0to1 (static_cast<float> (juce::jlimit (0, 16, choiceIdx))));
    updatingParamsFromCode = false;
}

// ── Gesture marking ───────────────────────────────────────────────────────────

void ArabicMaqamTunerProcessor::beginSliderGesture (int chromaticIndex)
{
    if (chromaticIndex < 0 || chromaticIndex >= kNumSlotParams)
        return;

    // Map chromatic index to tonic-relative slot index
    const int slotIdx = chromaticToSlot (chromaticIndex);
    auto* param = slotParams[(size_t) slotIdx];
    if (param != nullptr)
        param->beginChangeGesture();
}

void ArabicMaqamTunerProcessor::endSliderGesture (int chromaticIndex)
{
    if (chromaticIndex < 0 || chromaticIndex >= kNumSlotParams)
        return;

    // Map chromatic index to tonic-relative slot index
    const int slotIdx = chromaticToSlot (chromaticIndex);
    auto* param = slotParams[(size_t) slotIdx];
    if (param != nullptr)
        param->endChangeGesture();
}

void ArabicMaqamTunerProcessor::beginPresetGesture()
{
    if (presetParam != nullptr)
        presetParam->beginChangeGesture();
}

void ArabicMaqamTunerProcessor::endPresetGesture()
{
    if (presetParam != nullptr)
        presetParam->endChangeGesture();
}

// ── Maqam/scroll state ───────────────────────────────────────────────────────

void ArabicMaqamTunerProcessor::setStartMidi (double startMidi)
{
    currentStartMidi = startMidi;
}

void ArabicMaqamTunerProcessor::setMidiPresetNote (int presetIdx, int midiNote)
{
    if (presetIdx >= 0 && presetIdx < 16)
    {
        midiPresetNotes[(size_t) presetIdx].store (juce::jlimit (-1, 127, midiNote), std::memory_order_relaxed);
        saveSettingsToDisk();
    }
}

int ArabicMaqamTunerProcessor::getMidiPresetNote (int presetIdx) const
{
    if (presetIdx >= 0 && presetIdx < 16)
        return midiPresetNotes[(size_t) presetIdx].load (std::memory_order_relaxed);
    return -1;
}

void ArabicMaqamTunerProcessor::setMidiPresetChannel (int channel)
{
    // 0 = any channel, 1-16 = specific channel
    midiPresetChannel.store (juce::jlimit (0, 16, channel), std::memory_order_relaxed);
    saveSettingsToDisk();
}

int ArabicMaqamTunerProcessor::consumePendingMidiPreset()
{
    return pendingMidiPreset.exchange (-1, std::memory_order_relaxed);
}

void ArabicMaqamTunerProcessor::startMidiLearn (int presetIdx)
{
    if (presetIdx >= 0 && presetIdx < 16)
    {
        midiLearnTargetPreset.store (presetIdx, std::memory_order_relaxed);
        notifyTuningChanged();  // Update UI to show learning animation immediately
    }
}

void ArabicMaqamTunerProcessor::cancelMidiLearn()
{
    midiLearnTargetPreset.store (-1, std::memory_order_relaxed);
    notifyTuningChanged();  // Update UI to hide learning animation
}

void ArabicMaqamTunerProcessor::clearMidiPresetNote (int presetIdx)
{
    if (presetIdx >= 0 && presetIdx < 16)
    {
        midiPresetNotes[(size_t) presetIdx].store (-1, std::memory_order_relaxed);
        saveSettingsToDisk();
        notifyTuningChanged();  // Update UI to remove MIDI badge immediately
    }
}

void ArabicMaqamTunerProcessor::clearAllMidiPresetNotes()
{
    for (int i = 0; i < 16; ++i)
        midiPresetNotes[(size_t) i].store (-1, std::memory_order_relaxed);
    saveSettingsToDisk();
}

// ── Direct MIDI device input ─────────────────────────────────────────────────

juce::StringArray ArabicMaqamTunerProcessor::getAvailableMidiDevices() const
{
    juce::StringArray devices;
    devices.add ("None");  // First option to disable
    for (const auto& info : juce::MidiInput::getAvailableDevices())
        devices.add (info.name);
    return devices;
}

juce::String ArabicMaqamTunerProcessor::getMidiPresetDevice() const
{
    return midiPresetDeviceName;
}

void ArabicMaqamTunerProcessor::setMidiPresetDevice (const juce::String& deviceName)
{
    // Close existing input if any
    if (midiPresetInput)
    {
        midiPresetInput->stop();
        midiPresetInput.reset();
    }

    midiPresetDeviceName = deviceName;

    if (deviceName.isEmpty() || deviceName == "None")
        return;

    // Find and open the device
    for (const auto& info : juce::MidiInput::getAvailableDevices())
    {
        if (info.name == deviceName)
        {
            midiPresetInput = juce::MidiInput::openDevice (info.identifier, this);
            if (midiPresetInput)
            {
                midiPresetInput->start();
                DBG ("Opened MIDI device for preset triggering: " + deviceName);
            }
            else
            {
                DBG ("Failed to open MIDI device: " + deviceName);
            }
            break;
        }
    }
}

void ArabicMaqamTunerProcessor::handleIncomingMidiMessage (juce::MidiInput* /*source*/,
                                                            const juce::MidiMessage& message)
{
    if (! message.isNoteOn())
        return;

    const int channel  = midiPresetChannel.load (std::memory_order_relaxed);
    const int note     = message.getNoteNumber();
    const int midiCh   = message.getChannel();  // 1-16

    // Check channel filter first
    if (channel != 0 && midiCh != channel)
        return;

    // Check if we're in MIDI learn mode
    const int learnTarget = midiLearnTargetPreset.load (std::memory_order_relaxed);
    if (learnTarget >= 0 && learnTarget < 16)
    {
        // Assign this note to the target preset
        midiPresetNotes[(size_t) learnTarget].store (note, std::memory_order_relaxed);
        midiLearnTargetPreset.store (-1, std::memory_order_relaxed);  // Exit learn mode

        // Notify UI and save settings (we're on MIDI thread, so use callAsync)
        std::weak_ptr<std::atomic<bool>> weak (alive);
        juce::MessageManager::callAsync ([this, weak]
        {
            if (! isAlive (weak)) return;
            saveSettingsToDisk();
            if (onTuningStateChanged) onTuningStateChanged();
        });
        return;
    }

    // Check if this note is mapped to any preset
    for (int i = 0; i < 16; ++i)
    {
        if (midiPresetNotes[(size_t) i].load (std::memory_order_relaxed) == note)
        {
            pendingMidiPreset.store (i, std::memory_order_relaxed);
            return;
        }
    }
}

// ── Disk persistence ─────────────────────────────────────────────────────────

static juce::File getTanghimDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Tanghim");
}

void ArabicMaqamTunerProcessor::saveSettingsToDisk() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("tuningSystemId", currentSystemId);
    obj->setProperty ("startingNote",   currentStartingNote);

    // Save MIDI preset mappings
    juce::Array<juce::var> midiNotesArr;
    for (int i = 0; i < 16; ++i)
        midiNotesArr.add (juce::var (midiPresetNotes[(size_t) i].load (std::memory_order_relaxed)));
    obj->setProperty ("midiPresetNotes", midiNotesArr);
    obj->setProperty ("midiPresetChannel", midiPresetChannel.load (std::memory_order_relaxed));
    obj->setProperty ("midiPresetDevice", midiPresetDeviceName);

    const auto dir = getTanghimDir();
    dir.createDirectory();
    dir.getChildFile ("settings.json")
       .replaceWithText (juce::JSON::toString (juce::var (obj)));
}

void ArabicMaqamTunerProcessor::clearCache()
{
    dataCache.clearAll();

    // Reset all tuning state to initial load state
    currentSystemId.clear();
    currentStartingNote.clear();
    currentMaqamList.clear();
    currentMaqamDisplay.clear();
    currentTonicDisplay.clear();
    currentTonicEnglish.clear();
    currentTonicSolfege.clear();
    currentMaqamId.clear();
    currentTranspositionIdx = -1;
    currentActivePresetIdx  = -1;
    currentDegreeNames.clear();
    currentDegreeIpnRefs.fill ({});
    currentDegreeSolfegeRefs.fill ({});
    currentTranspositionIdMap.clear();

    // Reset slider state
    activeTuningState.clearPerNoteOverrides();
    for (int i = 0; i < 12; ++i)
    {
        auto& slot = activeTuningState.slots[(size_t) i];
        slot.variants.clear();
        slot.selectedIndex = 0;
        slot.centsOffset   = 0.0;
    }

    // Update MTS-ESP tuning table and notify UI
    tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());
    notifyTuningChanged();

    // Re-fetch tuning systems list from API (cache is empty now)
    std::weak_ptr<std::atomic<bool>> weak (alive);
    apiClient.fetchTuningSystems (
        [this, weak] (std::vector<TuningSystem> systems)
        {
            if (! isAlive (weak)) return;
            dataCache.setTuningSystemsList (std::move (systems));
            if (onTuningSystemsLoaded) onTuningSystemsLoaded();
        },
        [this, weak] (juce::String err)
        {
            if (! isAlive (weak)) return;
            if (onStatusMessage) onStatusMessage ("Network error: " + err);
        });

    DBG ("Cache cleared — reset to init state");
}

void ArabicMaqamTunerProcessor::loadSettingsFromDisk()
{
    const auto file = getTanghimDir().getChildFile ("settings.json");
    if (! file.existsAsFile()) return;

    auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (auto* obj = parsed.getDynamicObject())
    {
        const juce::String sysId = obj->getProperty ("tuningSystemId").toString();
        const juce::String note  = obj->getProperty ("startingNote").toString();

        if (sysId.isNotEmpty() && note.isNotEmpty())
        {
            currentSystemId     = sysId;
            currentStartingNote = note;
            DBG ("loadSettingsFromDisk: system=" + sysId + " note=" + note);
        }

        // Load MIDI preset mappings
        auto midiNotesProp = obj->getProperty ("midiPresetNotes");
        if (midiNotesProp.isArray())
        {
            auto* arr = midiNotesProp.getArray();
            for (int i = 0; i < juce::jmin (16, arr->size()); ++i)
                midiPresetNotes[(size_t) i].store (static_cast<int> ((*arr)[i]), std::memory_order_relaxed);
        }

        auto channelProp = obj->getProperty ("midiPresetChannel");
        if (! channelProp.isVoid())
            midiPresetChannel.store (static_cast<int> (channelProp), std::memory_order_relaxed);

        auto deviceProp = obj->getProperty ("midiPresetDevice");
        if (! deviceProp.isVoid())
        {
            midiPresetDeviceName = deviceProp.toString();
            setMidiPresetDevice (midiPresetDeviceName);  // Actually open the device
        }
    }
}

void ArabicMaqamTunerProcessor::savePresetsToDisk() const
{
    juce::Array<juce::var> arr;
    for (int i = 0; i < 16; ++i)
    {
        const auto& p = presets[(size_t) i];
        if (! p.isAssigned)
        {
            arr.add (juce::var());  // null
            continue;
        }

        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty ("maqamId",         p.maqamIdName);
        obj->setProperty ("maqamDisplay",    p.maqamDisplayName);
        obj->setProperty ("baseMaqamId",     p.baseMaqamIdName);
        obj->setProperty ("isTransposed",    p.isTransposed);
        obj->setProperty ("tonicNote",       p.tonicNoteName);
        obj->setProperty ("tonicIpn",        p.tonicIpnRef);
        obj->setProperty ("tonicSolfege",    p.tonicSolfege);
        obj->setProperty ("setIdx",          p.pitchClassSetIndex);

        juce::Array<juce::var> sp;
        for (int j = 0; j < 12; ++j)
            sp.add (p.sliderPositions[(size_t) j]);
        obj->setProperty ("sliderPositions", sp);

        juce::Array<juce::var> co;
        for (int j = 0; j < 12; ++j)
            co.add (p.centsOffsets[(size_t) j]);
        obj->setProperty ("centsOffsets", co);

        juce::Array<juce::var> dn;
        for (const auto& d : p.degreeNames)
            dn.add (d);
        obj->setProperty ("degreeNames", dn);

        obj->setProperty ("tuningSystemId", p.tuningSystemId);
        obj->setProperty ("startingNote",   p.startingNote);

        arr.add (juce::var (obj.release()));
    }

    const auto dir = getTanghimDir();
    dir.createDirectory();
    dir.getChildFile ("presets.json")
       .replaceWithText (juce::JSON::toString (juce::var (arr)));
}

void ArabicMaqamTunerProcessor::loadPresetsFromDisk()
{
    const auto file = getTanghimDir().getChildFile ("presets.json");
    if (! file.existsAsFile()) return;

    auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (auto* arr = parsed.getArray())
    {
        for (int i = 0; i < juce::jmin (16, arr->size()); ++i)
        {
            const auto& item = (*arr)[i];
            auto* obj = item.getDynamicObject();
            if (obj == nullptr) continue;  // null = unassigned

            auto& p = presets[(size_t) i];
            p.isAssigned         = true;
            p.maqamIdName        = obj->getProperty ("maqamId").toString();
            p.maqamDisplayName   = obj->getProperty ("maqamDisplay").toString();
            p.baseMaqamIdName    = obj->getProperty ("baseMaqamId").toString();
            p.isTransposed       = (bool) obj->getProperty ("isTransposed");
            p.tonicNoteName      = obj->getProperty ("tonicNote").toString();
            p.tonicIpnRef        = obj->getProperty ("tonicIpn").toString();
            p.tonicSolfege       = obj->getProperty ("tonicSolfege").toString();
            p.pitchClassSetIndex = (int) obj->getProperty ("setIdx");

            if (auto* spArr = obj->getProperty ("sliderPositions").getArray())
                for (int j = 0; j < juce::jmin (12, spArr->size()); ++j)
                    p.sliderPositions[(size_t) j] = (int) (*spArr)[j];

            if (auto* coArr = obj->getProperty ("centsOffsets").getArray())
                for (int j = 0; j < juce::jmin (12, coArr->size()); ++j)
                    p.centsOffsets[(size_t) j] = (double) (*coArr)[j];

            p.degreeNames.clear();
            if (auto* dnArr = obj->getProperty ("degreeNames").getArray())
                for (const auto& d : *dnArr)
                    if (d.toString().isNotEmpty())
                        p.degreeNames.push_back (d.toString());

            p.tuningSystemId = obj->getProperty ("tuningSystemId").toString();
            p.startingNote   = obj->getProperty ("startingNote").toString();
        }
        DBG ("loadPresetsFromDisk: loaded presets from disk");
    }
}

// ── Plugin factory ────────────────────────────────────────────────────────────

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArabicMaqamTunerProcessor();
}
