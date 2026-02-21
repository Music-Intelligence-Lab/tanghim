#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "api/ApiResponseParser.h"
#include <cmath>
#include <limits>
#include <map>
#include <set>

ArabicMaqamTunerProcessor::ArabicMaqamTunerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      updateChecker (apiClient, dataCache, std::weak_ptr<std::atomic<bool>> (alive))
{
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

            tuningEngine.updateTuning (activeTuningState, buildScaleName());
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

        tuningEngine.updateTuning (activeTuningState, buildScaleName());
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

    tuningEngine.updateTuning (activeTuningState, buildScaleName());
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

    // Update MTS-ESP immediately (live pitch change) — no WebView push
    tuningEngine.updateTuning (activeTuningState, buildScaleName());
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

    tuningEngine.updateTuning (activeTuningState, buildScaleName());
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

    tuningEngine.updateTuning (activeTuningState, buildScaleName());
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
        applyMaqamDegrees (found->degrees);

        // Store degree names from base degrees
        currentDegreeNames.clear();
        for (const auto& name : found->degrees.ascending)
            currentDegreeNames.push_back (name);
    }
    else
    {
        const auto& t = found->transpositions[(size_t) transpositionIndex];
        currentTonicDisplay = t.tonicDisplay;
        tonicId = t.tonicId;
        applyMaqamDegrees (t.degrees);

        // Store degree names from transposition degrees
        currentDegreeNames.clear();
        for (const auto& name : t.degrees.ascending)
            currentDegreeNames.push_back (name);
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
}

void ArabicMaqamTunerProcessor::clearPreset (int idx)
{
    if (idx < 0 || idx >= 16) return;
    presets[(size_t) idx].clear();

    if (currentActivePresetIdx == idx)
        currentActivePresetIdx = -1;

    savePresetsToDisk();
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

    // Reset all slots to default (index 0) before applying maqam degrees,
    // so we don't accumulate selections from previously applied maqamat
    activeTuningState.clearPerNoteOverrides();
    for (int ci = 0; ci < 12; ++ci)
    {
        auto& sl = activeTuningState.slots[(size_t) ci];
        sl.selectedIndex = 0;
        if (const auto* v = sl.selectedVariant())
            sl.centsOffset = v->midiCentsDeviation;
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

    tuningEngine.updateTuning (activeTuningState, buildScaleName());
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

// ── Maqam/scroll state ───────────────────────────────────────────────────────

void ArabicMaqamTunerProcessor::setStartMidi (double startMidi)
{
    currentStartMidi = startMidi;
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

    const auto dir = getTanghimDir();
    dir.createDirectory();
    dir.getChildFile ("settings.json")
       .replaceWithText (juce::JSON::toString (juce::var (obj)));
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
