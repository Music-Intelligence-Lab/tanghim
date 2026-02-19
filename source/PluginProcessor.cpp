#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "api/ApiResponseParser.h"
#include <map>
#include <set>

ArabicMaqamTunerProcessor::ArabicMaqamTunerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      updateChecker (apiClient, dataCache, std::weak_ptr<std::atomic<bool>> (alive))
{
    dataCache.loadFromDisk();

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
    // Slider positions
    auto slidersNode = juce::ValueTree ("SliderPositions");
    for (int i = 0; i < 12; ++i)
        slidersNode.setProperty ("s" + juce::String (i),
                                 activeTuningState.slots[(size_t) i].selectedIndex,
                                 nullptr);
    state.addChild (slidersNode, -1, nullptr);

    // Presets
    auto presetsNode = juce::ValueTree ("Presets");
    for (int i = 0; i < 12; ++i)
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
        n.setProperty ("setIdx",          p.pitchClassSetIndex, nullptr);
        for (int j = 0; j < 12; ++j)
            n.setProperty ("sp" + juce::String (j), p.sliderPositions[(size_t) j], nullptr);

        // Degree names (for compatibility checking across tuning systems)
        juce::String degStr;
        for (size_t d = 0; d < p.degreeNames.size(); ++d)
        {
            if (d > 0) degStr += "|";
            degStr += p.degreeNames[d];
        }
        n.setProperty ("degreeNames", degStr, nullptr);

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

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, dest);
}

void ArabicMaqamTunerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;

    // Restore presets immediately (they're just data)
    auto presetsNode = state.getChildWithName ("Presets");
    for (int i = 0; i < 12; ++i)
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
        p.pitchClassSetIndex = (int) n.getProperty ("setIdx", -1);
        for (int j = 0; j < 12; ++j)
            p.sliderPositions[(size_t) j] = (int) n.getProperty ("sp" + juce::String (j), 0);

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
    }

    // Restore tuning system — async (may need API fetch)
    const juce::String sysId    = state.getProperty ("tuningSystemId").toString();
    const juce::String startNote = state.getProperty ("startingNote").toString();

    auto slidersNode = state.getChildWithName ("SliderPositions");
    std::array<int, 12> savedPositions;
    for (int i = 0; i < 12; ++i)
        savedPositions[(size_t) i] = (int) slidersNode.getProperty ("s" + juce::String (i), 0);

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
        loadTuningSystem (sysId, startNote, [this, weak, savedPositions, savedPerNote] ()
        {
            if (! isAlive (weak)) return;
            for (int i = 0; i < 12; ++i)
                setSliderVariant (i, savedPositions[(size_t) i]);

            // Restore per-note overrides after slider positions are set
            activeTuningState.perNoteVariantOverrides = savedPerNote;
            tuningEngine.updateTuning (activeTuningState, buildScaleName());
            notifyTuningChanged();
        });
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

        // Rebuild all slots with new variants, default to first variant
        activeTuningState.clearPerNoteOverrides();
        for (int i = 0; i < 12; ++i)
        {
            auto& slot = activeTuningState.slots[(size_t) i];
            slot.ipnReference  = kChromaticIpnRefs[i];
            slot.variants      = variants[(size_t) i];
            slot.selectedIndex = 0;
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

        tuningEngine.updateTuning (activeTuningState, buildScaleName());
        if (onStatusMessage) onStatusMessage (buildScaleName());
        notifyTuningChanged();
        if (onComplete) onComplete();

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

    // Clear per-note overrides for this chromatic position (all-octaves reset)
    for (int midi = chromaticIndex; midi < 128; midi += 12)
        activeTuningState.perNoteVariantOverrides[(size_t) midi] = -1;

    tuningEngine.updateTuning (activeTuningState, buildScaleName());
    notifyTuningChanged();
}

void ArabicMaqamTunerProcessor::setNoteVariant (int midiNote, int variantIndex)
{
    if (midiNote < 0 || midiNote >= 128) return;
    const int chromaticIdx = midiNote % 12;
    const auto& slot = activeTuningState.slots[(size_t) chromaticIdx];
    activeTuningState.perNoteVariantOverrides[(size_t) midiNote] =
        juce::jlimit (0, std::max (0, slot.variantCount() - 1), variantIndex);
    tuningEngine.updateTuning (activeTuningState, buildScaleName());
    notifyTuningChanged();
}

void ArabicMaqamTunerProcessor::applyPreset (int idx)
{
    if (idx < 0 || idx >= 12) return;
    const auto& preset = presets[(size_t) idx];
    if (! preset.isAssigned) return;

    activeTuningState.clearPerNoteOverrides();
    for (int i = 0; i < 12; ++i)
        setSliderVariant (i, preset.sliderPositions[(size_t) i]);
}

void ArabicMaqamTunerProcessor::assignPreset (int idx,
                                               const juce::String& maqamId,
                                               const juce::String& maqamDisplay,
                                               const juce::String& baseMaqamId,
                                               bool isTransposed,
                                               const juce::String& tonicNote,
                                               const juce::String& tonicIpn,
                                               int setIdx,
                                               const std::array<int, 12>& positions,
                                               const std::vector<juce::String>& degreeNames)
{
    if (idx < 0 || idx >= 12) return;
    auto& p              = presets[(size_t) idx];
    p.isAssigned         = true;
    p.maqamIdName        = maqamId;
    p.maqamDisplayName   = maqamDisplay;
    p.baseMaqamIdName    = baseMaqamId;
    p.isTransposed       = isTransposed;
    p.tonicNoteName      = tonicNote;
    p.tonicIpnRef        = tonicIpn;
    p.pitchClassSetIndex = setIdx;
    p.sliderPositions    = positions;
    p.degreeNames        = degreeNames;
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

    // Choose base degrees or a specific transposition
    if (transpositionIndex < 0 || transpositionIndex >= (int) found->transpositions.size())
        applyMaqamDegrees (found->degrees);
    else
        applyMaqamDegrees (found->transpositions[(size_t) transpositionIndex].degrees);
}

void ArabicMaqamTunerProcessor::clearPreset (int idx)
{
    if (idx >= 0 && idx < 12) presets[(size_t) idx].clear();
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

const std::array<MaqamPreset, 12>& ArabicMaqamTunerProcessor::getPresets() const
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
        activeTuningState.slots[(size_t) ci].selectedIndex = 0;

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
    }
}

void ArabicMaqamTunerProcessor::notifyTuningChanged()
{
    if (onTuningStateChanged) onTuningStateChanged();
}

juce::String ArabicMaqamTunerProcessor::buildScaleName() const
{
    if (currentSystemId.isEmpty()) return "Tanghim";
    const auto& systems = dataCache.getTuningSystemsList();
    for (const auto& ts : systems)
        if (ts.id == currentSystemId)
            return ts.shortName;
    return currentSystemId;
}

// ── Plugin factory ────────────────────────────────────────────────────────────

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArabicMaqamTunerProcessor();
}
