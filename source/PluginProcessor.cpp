#include "PluginProcessor.h"
#include "editor/TanghimNativeEditor.h"
#include "api/ApiResponseParser.h"
#include <cmath>
#include <limits>
#include <map>
#include <set>

// ── APVTS parameter layout ────────────────────────────────────────────────────

juce::AudioProcessorValueTreeState::ParameterLayout TanghimProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 12 slot params: slot_0 through slot_11 (±150 cents range to match UI)
    for (int i = 0; i < kNumSlotParams; ++i)
    {
        auto id = juce::ParameterID ("slot_" + juce::String (i), 1);
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            id, "Slider " + juce::String (i + 1),
            juce::NormalisableRange<float> (-150.0f, 150.0f, 0.01f),
            0.0f));
    }

    // Reference frequency offset (±700 cents = a perfect fifth each direction)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("ref_freq", 1), "Ref Freq",
        juce::NormalisableRange<float> (-700.0f, 700.0f, 0.01f),
        0.0f));

    // Preset param: "None" + presets 1..kNumMaqamPresets (matches UI grid)
    juce::StringArray presetChoices;
    presetChoices.add ("None");
    for (int i = 1; i <= kNumMaqamPresets; ++i)
        presetChoices.add (juce::String (i));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID ("preset", 1), "Preset",
        presetChoices, 0));

    return { params.begin(), params.end() };
}

TanghimProcessor::TanghimProcessor()
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
    for (int i = 0; i < kNumMaqamPresets; ++i)
        midiPresetNotes[i].store (-1, std::memory_order_relaxed);

    // Initialize heptatonic map to identity (delta 0 = no remapping)
    for (int i = 0; i < 12; ++i)
        heptMap[i].store (0, std::memory_order_relaxed);
    for (int i = 0; i < 128; ++i)
        activeHeptNotes[i].store (-1, std::memory_order_relaxed);

    // Register as listener for all params
    for (int i = 0; i < kNumSlotParams; ++i)
        apvts.addParameterListener ("slot_" + juce::String (i), this);
    apvts.addParameterListener ("preset", this);
    apvts.addParameterListener ("ref_freq", this);
    dataCache.loadFromDisk();
    loadPresetsFromDisk();
    loadSettingsFromDisk();

    // Deferred MIDI device re-open: CoreMIDI connections established during
    // plugin construction may be stale or fail (DAW init timing, Rosetta bridge).
    // Schedule a fresh open after the constructor exits and the message loop runs.
    if (midiPresetDeviceName.isNotEmpty())
    {
        std::weak_ptr<std::atomic<bool>> weak (alive);
        juce::MessageManager::callAsync ([this, weak] ()
        {
            if (! isAlive (weak)) return;
            const auto device = midiPresetDeviceName;
            const auto deviceId = midiPresetDeviceId;
            if (device.isEmpty()) return;

            // Close any stale connection from constructor-time init
            if (midiPresetInput)
            {
                midiPresetInput->stop();
                midiPresetInput.reset();
            }
            midiPresetDeviceName.clear();
            midiPresetDeviceId = deviceId;  // Preserve identifier for setMidiPresetDevice
            setMidiPresetDevice (device);
        });
    }

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

TanghimProcessor::~TanghimProcessor()
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

void TanghimProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    oscillator.prepare (sampleRate);
}
void TanghimProcessor::releaseResources() {}

// ── Audio-thread slot automation polling ──────────────────────────────────────

void TanghimProcessor::pollSlotAutomation()
{
    uint16_t changedMask = 0;

    for (int s = 0; s < kNumSlotParams; ++s)
    {
        const float val = slotParams[s]->get();
        if (val == lastAudioSlotValues[(size_t) s])
            continue;

        lastAudioSlotValues[(size_t) s] = val;
        const int chromaticIdx = slotToChromatic (s);
        const double cents = juce::jlimit (-150.0, 150.0, (double) val);
        tuningEngine.updateSlotTuning (chromaticIdx, cents, referenceCentsOffset);
        changedMask |= uint16_t (1u << chromaticIdx);
    }

    if (changedMask == 0)
        return;

    if (heptEnabled.load (std::memory_order_relaxed))
        rebroadcastHeptMts();

    slotAutomationDirtyMask.fetch_or (changedMask, std::memory_order_relaxed);
}

void TanghimProcessor::processBlock (juce::AudioBuffer<float>& audio,
                                               juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();

    // Poll APVTS slot params at buffer rate — catches automation changes faster
    // than the message-thread parameterChanged callback (which fires at host UI
    // refresh rate, typically 30-60Hz). This ensures fast automation sweeps
    // reach extreme values without lag.
    pollSlotAutomation();

    const bool oscOn = oscillatorEnabled.load (std::memory_order_relaxed);

    // ── Fast idle path: no MIDI and no oscillator work → return immediately ──
    if (midi.isEmpty() && (! oscOn || ! oscillator.hasActiveVoices()))
        return;

    const bool heptOn = heptEnabled.load (std::memory_order_relaxed);
    const int presetCh = midiPresetChannel.load (std::memory_order_relaxed);

    // ── MIDI processing: heptatonic remapping + pitch bend tracking ──────────
    // Single pass handles hept note remapping, PB extraction for oscillator, and
    // PB stripping from output. PB messages are removed from the MIDI output
    // because MTS-ESP synths receive tuning via the frequency table — raw PB
    // at their own PB range (often 48st for MPE-capable synths) causes wild
    // pitch jumps. PB for downstream synths is handled by Tanghim Receivers.
    if (! midi.isEmpty())
    {
        juce::MidiBuffer processed;
        for (const auto metadata : midi)
        {
            auto msg = metadata.getMessage();

            // Program change → preset switching (PC 0-7 = presets 0-7)
            if (msg.isProgramChange())
            {
                const int pc = msg.getProgramChangeNumber();
                if (pc >= 0 && pc < 8)
                    pendingMidiPreset.store (pc, std::memory_order_relaxed);
                continue;
            }

            // Extract pitch bend for oscillator, strip from output
            if (msg.isPitchWheel())
            {
                currentPitchBend = msg.getPitchWheelValue();
                continue;
            }

            // Heptatonic note remapping
            if (heptOn)
            {
                if (msg.isNoteOn())
                {
                    const int rawNote = msg.getNoteNumber();
                    const int note = juce::jlimit (0, 127,
                        rawNote + heptMap[rawNote % 12].load (std::memory_order_relaxed));
                    activeHeptNotes[rawNote].store (note, std::memory_order_relaxed);
                    msg.setNoteNumber (note);
                }
                else if (msg.isNoteOff())
                {
                    const int rawNote = msg.getNoteNumber();
                    const int stored = activeHeptNotes[rawNote].load (std::memory_order_relaxed);
                    if (stored >= 0)
                    {
                        msg.setNoteNumber (stored);
                        activeHeptNotes[rawNote].store (-1, std::memory_order_relaxed);
                    }
                }
            }

            processed.addEvent (msg, metadata.samplePosition);
        }
        midi.swapWith (processed);
    }

    // Compute PB frequency multiplier for oscillator (±2 semitones)
    const double pbMultiplier = (currentPitchBend == 8192) ? 1.0
        : std::pow (2.0, (currentPitchBend - 8192) / 8192.0 * kPitchBendRangeSt / 12.0);

    // ── Note activity + oscillator triggering ────────────────────────────────
    for (const auto metadata : midi)
    {
        const auto msg  = metadata.getMessage();

        if (presetCh > 0 && msg.getChannel() == presetCh)
            continue;

        const auto note = msg.getNoteNumber();
        const auto word = note >> 5;
        const auto bit  = uint32_t (1u << (note & 31));
        if (msg.isNoteOn())
        {
            noteOnBits[word].fetch_or (bit, std::memory_order_relaxed);
            if (oscOn)
                oscillator.noteOn (note,
                    tuningEngine.getFrequencyForMidiNote (note) * pbMultiplier);
        }
        else if (msg.isNoteOff())
        {
            noteOffBits[word].fetch_or (bit, std::memory_order_relaxed);
            if (oscOn)
                oscillator.noteOff (note);
        }
    }

    // Generate reference oscillator audio if enabled and voices active
    if (oscOn && oscillator.hasActiveVoices())
    {
        // Snapshot frequency table under one lock (instead of per-voice lock)
        std::array<double, 128> freqSnapshot;
        tuningEngine.snapshotFrequencyTable (freqSnapshot);

        for (auto& v : oscillator.voices)
        {
            if (v.active && v.midiNote >= 0)
                v.updateFrequency (freqSnapshot[(size_t) v.midiNote] * pbMultiplier,
                                   oscillator.sampleRate);
        }

        // Block render into channel 0 via direct pointer (no addSample overhead)
        const int numSamples  = audio.getNumSamples();
        float* ch0 = audio.getWritePointer (0);
        oscillator.renderBlock (ch0, numSamples, 0.125f);

        // Copy channel 0 to remaining channels
        const int numChannels = audio.getNumChannels();
        for (int ch = 1; ch < numChannels; ++ch)
            juce::FloatVectorOperations::copy (audio.getWritePointer (ch), ch0, numSamples);
    }
}

void TanghimProcessor::processBlock (juce::AudioBuffer<double>& audio,
                                               juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    audio.clear();

    const bool oscOn = oscillatorEnabled.load (std::memory_order_relaxed);

    // ── Fast idle path: no MIDI and no oscillator work → return immediately ──
    if (midi.isEmpty() && (! oscOn || ! oscillator.hasActiveVoices()))
        return;

    const bool heptOn = heptEnabled.load (std::memory_order_relaxed);
    const int presetCh = midiPresetChannel.load (std::memory_order_relaxed);

    // ── MIDI processing: heptatonic remapping + pitch bend tracking ──────────
    if (! midi.isEmpty())
    {
        juce::MidiBuffer processed;
        for (const auto metadata : midi)
        {
            auto msg = metadata.getMessage();

            // Program change → preset switching (PC 0-7 = presets 0-7)
            if (msg.isProgramChange())
            {
                const int pc = msg.getProgramChangeNumber();
                if (pc >= 0 && pc < 8)
                    pendingMidiPreset.store (pc, std::memory_order_relaxed);
                continue;
            }

            if (msg.isPitchWheel())
            {
                currentPitchBend = msg.getPitchWheelValue();
                continue;
            }

            if (heptOn)
            {
                if (msg.isNoteOn())
                {
                    const int rawNote = msg.getNoteNumber();
                    const int note = juce::jlimit (0, 127,
                        rawNote + heptMap[rawNote % 12].load (std::memory_order_relaxed));
                    activeHeptNotes[rawNote].store (note, std::memory_order_relaxed);
                    msg.setNoteNumber (note);
                }
                else if (msg.isNoteOff())
                {
                    const int rawNote = msg.getNoteNumber();
                    const int stored = activeHeptNotes[rawNote].load (std::memory_order_relaxed);
                    if (stored >= 0)
                    {
                        msg.setNoteNumber (stored);
                        activeHeptNotes[rawNote].store (-1, std::memory_order_relaxed);
                    }
                }
            }

            processed.addEvent (msg, metadata.samplePosition);
        }
        midi.swapWith (processed);
    }

    const double pbMultiplier = (currentPitchBend == 8192) ? 1.0
        : std::pow (2.0, (currentPitchBend - 8192) / 8192.0 * kPitchBendRangeSt / 12.0);

    for (const auto metadata : midi)
    {
        const auto msg  = metadata.getMessage();

        if (presetCh > 0 && msg.getChannel() == presetCh)
            continue;

        const auto note = msg.getNoteNumber();
        const auto word = note >> 5;
        const auto bit  = uint32_t (1u << (note & 31));
        if (msg.isNoteOn())
        {
            noteOnBits[word].fetch_or (bit, std::memory_order_relaxed);
            if (oscOn)
                oscillator.noteOn (note,
                    tuningEngine.getFrequencyForMidiNote (note) * pbMultiplier);
        }
        else if (msg.isNoteOff())
        {
            noteOffBits[word].fetch_or (bit, std::memory_order_relaxed);
            if (oscOn)
                oscillator.noteOff (note);
        }
    }

    if (oscOn && oscillator.hasActiveVoices())
    {
        std::array<double, 128> freqSnapshot;
        tuningEngine.snapshotFrequencyTable (freqSnapshot);

        for (auto& v : oscillator.voices)
        {
            if (v.active && v.midiNote >= 0)
                v.updateFrequency (freqSnapshot[(size_t) v.midiNote] * pbMultiplier,
                                   oscillator.sampleRate);
        }

        const int numSamples  = audio.getNumSamples();
        double* ch0 = audio.getWritePointer (0);
        oscillator.renderBlock (ch0, numSamples, 0.125);

        const int numChannels = audio.getNumChannels();
        for (int ch = 1; ch < numChannels; ++ch)
            juce::FloatVectorOperations::copy (audio.getWritePointer (ch), ch0, numSamples);
    }
}

juce::AudioProcessorEditor* TanghimProcessor::createEditor()
{
    return new TanghimNativeEditor (*this);
}

// ── State persistence ─────────────────────────────────────────────────────────

void TanghimProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = juce::ValueTree ("TanghimState");
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
    for (int i = 0; i < kNumMaqamPresets; ++i)
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

    // Per-note cents overrides (sparse: only save non-NaN entries)
    auto perNoteCentsNode = juce::ValueTree ("PerNoteCentsOverrides");
    for (int i = 0; i < 128; ++i)
    {
        if (activeTuningState.hasPerNoteCentsOverride (i))
            perNoteCentsNode.setProperty ("c" + juce::String (i),
                                          activeTuningState.perNoteCentsOverrides[(size_t) i], nullptr);
    }
    state.addChild (perNoteCentsNode, -1, nullptr);

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
    for (int i = 0; i < kNumMaqamPresets; ++i)
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

    // Internal reference oscillator
    state.setProperty ("oscillatorEnabled", oscillatorEnabled.load (std::memory_order_relaxed), nullptr);

    // Heptatonic keyboard mode
    state.setProperty ("heptEnabled", heptEnabled.load (std::memory_order_relaxed), nullptr);

    // Include APVTS state as a child
    state.addChild (apvts.copyState(), -1, nullptr);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, dest);
}

void TanghimProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;

    sessionRecallInProgress = true;

    // Restore presets immediately (they're just data)
    auto presetsNode = state.getChildWithName ("Presets");
    for (int i = 0; i < kNumMaqamPresets; ++i)
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
    const int savedPresetIdx           = juce::jlimit (-1, kNumMaqamPresets - 1,
                                                      (int) state.getProperty ("activePresetIndex",  -1));
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
        for (int i = 0; i < juce::jmin (kNumMaqamPresets, notes.size()); ++i)
            midiPresetNotes[(size_t) i].store (notes[i].getIntValue(), std::memory_order_relaxed);
    }

    auto midiChanProp = state.getProperty ("midiPresetChannel", juce::var());
    if (! midiChanProp.isVoid())
        midiPresetChannel.store ((int) midiChanProp, std::memory_order_relaxed);

    auto midiDeviceProp = state.getProperty ("midiPresetDevice", juce::var());
    if (! midiDeviceProp.isVoid())
    {
        const auto sessionDevice = midiDeviceProp.toString();
        // Only override if session state has a device; don't let an empty
        // session value clear a working device loaded from settings.json
        if (sessionDevice.isNotEmpty())
            setMidiPresetDevice (sessionDevice);
    }

    // Restore tonic chromatic index
    auto tonicChromProp = state.getProperty ("tonicChromatic", juce::var());
    if (! tonicChromProp.isVoid())
        currentTonicChromatic = juce::jlimit (0, 11, (int) tonicChromProp);

    // Restore reference frequency offset
    auto refCentsProp = state.getProperty ("referenceCentsOffset", juce::var());
    if (! refCentsProp.isVoid())
        referenceCentsOffset = juce::jlimit (-700.0, 700.0, (double) refCentsProp);

    // Restore oscillator enabled state
    auto oscEnabledProp = state.getProperty ("oscillatorEnabled", juce::var());
    if (! oscEnabledProp.isVoid())
        oscillatorEnabled.store ((bool) oscEnabledProp, std::memory_order_relaxed);

    // Restore heptatonic mode
    auto heptEnabledProp = state.getProperty ("heptEnabled", juce::var());
    if (! heptEnabledProp.isVoid())
        heptEnabled.store ((bool) heptEnabledProp, std::memory_order_relaxed);

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

    // Read per-note cents overrides (sparse)
    auto perNoteCentsNode = state.getChildWithName ("PerNoteCentsOverrides");
    std::array<double, 128> savedPerNoteCents;
    for (auto& v : savedPerNoteCents) v = std::numeric_limits<double>::quiet_NaN();
    if (perNoteCentsNode.isValid())
    {
        for (int i = 0; i < 128; ++i)
        {
            auto prop = perNoteCentsNode.getProperty ("c" + juce::String (i), juce::var());
            if (! prop.isVoid())
                savedPerNoteCents[(size_t) i] = (double) prop;
        }
    }

    if (sysId.isNotEmpty() && startNote.isNotEmpty())
    {
        std::weak_ptr<std::atomic<bool>> weak (alive);
        loadTuningSystem (sysId, startNote,
            [this, weak, savedPositions, savedCentsOffsets, savedPerNote, savedPerNoteCents,
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
            activeTuningState.perNoteCentsOverrides = savedPerNoteCents;

            // Restore maqam selection state
            currentMaqamId           = savedMaqamId;
            currentTranspositionIdx  = savedTransIdx;
            currentActivePresetIdx   = savedPresetIdx;
            currentStartMidi         = savedStartMidi;
            currentDegreeNames       = savedDegreeNames;

            // Populate IPN/solfège from maqam list enriched degree data
            currentDegreeIpnRefs.fill ({});
            currentDegreeSolfegeRefs.fill ({});
            if (savedMaqamId.isNotEmpty())
            {
                for (const auto& mle : currentMaqamList)
                {
                    if (mle.maqamId == savedMaqamId)
                    {
                        if (savedTransIdx >= 0 && savedTransIdx < (int) mle.transpositions.size())
                            applyDegreeIpnAndSolfege (mle.transpositions[(size_t) savedTransIdx].degrees);
                        else
                            applyDegreeIpnAndSolfege (mle.degrees);
                        break;
                    }
                }
            }

            hasRecalledSessionState  = true;
            sessionRecallInProgress  = false;

            // Sync APVTS params after session recall
            syncAllSlotParamsFromState();
            syncPresetParamFromState();

            rebuildHeptMap();
            updateTuningAndBroadcast();
            notifyTuningChanged();
        });
    }
    else
    {
        sessionRecallInProgress = false;
    }
}

// ── File-based state save/load (.tanghim files) ──────────────────────────────

juce::String TanghimProcessor::buildStateJson() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("format",  "tanghim-state");
    root->setProperty ("version", 1);

    root->setProperty ("tuningSystemId", currentSystemId);
    root->setProperty ("startingNote",   currentStartingNote);
    root->setProperty ("referenceFreqCents", referenceCentsOffset);

    // 12 slider slots
    juce::Array<juce::var> slotsArr;
    for (int i = 0; i < 12; ++i)
    {
        auto* s = new juce::DynamicObject();
        s->setProperty ("selectedIndex", activeTuningState.slots[(size_t) i].selectedIndex);
        s->setProperty ("centsOffset",   activeTuningState.slots[(size_t) i].centsOffset);
        slotsArr.add (juce::var (s));
    }
    root->setProperty ("sliders", slotsArr);

    // Maqam selection state
    if (currentMaqamId.isNotEmpty())
    {
        auto* maqObj = new juce::DynamicObject();
        maqObj->setProperty ("id",               currentMaqamId);
        maqObj->setProperty ("display",           currentMaqamDisplay);
        maqObj->setProperty ("tonicDisplay",      currentTonicDisplay);
        maqObj->setProperty ("tonicEnglish",      currentTonicEnglish);
        maqObj->setProperty ("tonicSolfege",      currentTonicSolfege);
        maqObj->setProperty ("transpositionIndex", currentTranspositionIdx);
        maqObj->setProperty ("tonicChromatic",    currentTonicChromatic);

        juce::Array<juce::var> degArr;
        for (const auto& name : currentDegreeNames)
            degArr.add (juce::var (name));
        maqObj->setProperty ("degreeNames", degArr);

        root->setProperty ("maqam", juce::var (maqObj));
    }

    // Presets (same structure as presets.json)
    juce::Array<juce::var> presArr;
    for (int i = 0; i < kNumMaqamPresets; ++i)
    {
        const auto& p = presets[(size_t) i];
        if (! p.isAssigned)
        {
            presArr.add (juce::var());  // null
            continue;
        }

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("maqamId",       p.maqamIdName);
        obj->setProperty ("maqamDisplay",  p.maqamDisplayName);
        obj->setProperty ("baseMaqamId",   p.baseMaqamIdName);
        obj->setProperty ("isTransposed",  p.isTransposed);
        obj->setProperty ("tonicNote",     p.tonicNoteName);
        obj->setProperty ("tonicIpn",      p.tonicIpnRef);
        obj->setProperty ("tonicSolfege",  p.tonicSolfege);
        obj->setProperty ("setIdx",        p.pitchClassSetIndex);

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

        presArr.add (juce::var (obj));
    }
    root->setProperty ("presets", presArr);

    // Per-note overrides (sparse: only non-default entries)
    auto* perNoteObj = new juce::DynamicObject();
    for (int i = 0; i < 128; ++i)
    {
        if (activeTuningState.perNoteVariantOverrides[(size_t) i] >= 0)
            perNoteObj->setProperty (juce::String (i), activeTuningState.perNoteVariantOverrides[(size_t) i]);
    }
    root->setProperty ("perNoteOverrides", juce::var (perNoteObj));

    root->setProperty ("activePresetIndex", currentActivePresetIdx);
    root->setProperty ("scrollPosition",    currentStartMidi);
    root->setProperty ("oscillatorEnabled", oscillatorEnabled.load (std::memory_order_relaxed));
    root->setProperty ("heptEnabled",       heptEnabled.load (std::memory_order_relaxed));

    return juce::JSON::toString (juce::var (root));
}

void TanghimProcessor::restoreStateFromJson (const juce::String& json)
{
    auto parsed = juce::JSON::parse (json);
    auto* root = parsed.getDynamicObject();
    if (root == nullptr) return;

    // Validate format
    if (root->getProperty ("format").toString() != "tanghim-state") return;

    // Restore presets immediately (just data)
    if (auto* presArr = root->getProperty ("presets").getArray())
    {
        for (int i = 0; i < juce::jmin (kNumMaqamPresets, presArr->size()); ++i)
        {
            const auto& item = (*presArr)[i];
            auto* obj = item.getDynamicObject();
            if (obj == nullptr)
            {
                presets[(size_t) i] = MaqamPreset();
                continue;
            }

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
    }
    savePresetsToDisk();

    // Capture maqam state for restoration after loadTuningSystem (which clears it)
    juce::String savedMaqamId;
    juce::String savedMaqamDisplay;
    juce::String savedTonicDisplay;
    juce::String savedTonicEnglish;
    juce::String savedTonicSolfege;
    int savedTransIdx           = -1;
    int savedPresetIdx          = -1;
    double savedStartMidi       = 48.0;
    int savedTonicChromatic     = 0;
    std::vector<juce::String> savedDegreeNames;

    if (auto* maqObj = root->getProperty ("maqam").getDynamicObject())
    {
        savedMaqamDisplay   = maqObj->getProperty ("display").toString();
        savedTonicDisplay   = maqObj->getProperty ("tonicDisplay").toString();
        savedTonicEnglish   = maqObj->getProperty ("tonicEnglish").toString();
        savedTonicSolfege   = maqObj->getProperty ("tonicSolfege").toString();

        savedMaqamId        = maqObj->getProperty ("id").toString();
        savedTransIdx       = (int) maqObj->getProperty ("transpositionIndex");
        savedTonicChromatic = (int) maqObj->getProperty ("tonicChromatic");

        if (auto* degArr = maqObj->getProperty ("degreeNames").getArray())
            for (const auto& d : *degArr)
                if (d.toString().isNotEmpty())
                    savedDegreeNames.push_back (d.toString());
    }

    savedPresetIdx = juce::jlimit (-1, kNumMaqamPresets - 1, (int) root->getProperty ("activePresetIndex"));
    savedStartMidi = (double) root->getProperty ("scrollPosition");

    // Restore reference freq, oscillator, hept
    const double savedRefCents = juce::jlimit (-700.0, 700.0,
        (double) root->getProperty ("referenceFreqCents"));
    const bool savedOsc  = (bool) root->getProperty ("oscillatorEnabled");
    const bool savedHept = (bool) root->getProperty ("heptEnabled");

    // Read slider data
    std::array<int, 12> savedPositions;
    std::array<double, 12> savedCentsOffsets;
    savedPositions.fill (0);
    savedCentsOffsets.fill (std::numeric_limits<double>::quiet_NaN());
    if (auto* slidersArr = root->getProperty ("sliders").getArray())
    {
        for (int i = 0; i < juce::jmin (12, slidersArr->size()); ++i)
        {
            if (auto* sObj = (*slidersArr)[i].getDynamicObject())
            {
                savedPositions[(size_t) i]    = (int) sObj->getProperty ("selectedIndex");
                savedCentsOffsets[(size_t) i]  = (double) sObj->getProperty ("centsOffset");
            }
        }
    }

    // Read per-note overrides
    std::array<int, 128> savedPerNote;
    savedPerNote.fill (-1);
    if (auto* pnObj = root->getProperty ("perNoteOverrides").getDynamicObject())
    {
        for (int i = 0; i < 128; ++i)
        {
            auto prop = pnObj->getProperty (juce::String (i));
            if (! prop.isVoid())
                savedPerNote[(size_t) i] = (int) prop;
        }
    }

    // Read per-note cents overrides
    std::array<double, 128> savedPerNoteCents;
    for (auto& v : savedPerNoteCents) v = std::numeric_limits<double>::quiet_NaN();
    if (auto* pcObj = root->getProperty ("perNoteCentsOverrides").getDynamicObject())
    {
        for (int i = 0; i < 128; ++i)
        {
            auto prop = pcObj->getProperty (juce::String (i));
            if (! prop.isVoid())
                savedPerNoteCents[(size_t) i] = (double) prop;
        }
    }

    // Restore tuning system (async — may need API fetch)
    const juce::String sysId     = root->getProperty ("tuningSystemId").toString();
    const juce::String startNote = root->getProperty ("startingNote").toString();

    oscillatorEnabled.store (savedOsc, std::memory_order_relaxed);
    heptEnabled.store (savedHept, std::memory_order_relaxed);

    if (sysId.isNotEmpty() && startNote.isNotEmpty())
    {
        std::weak_ptr<std::atomic<bool>> weak (alive);
        loadTuningSystem (sysId, startNote,
            [this, weak, savedPositions, savedCentsOffsets, savedPerNote, savedPerNoteCents,
             savedMaqamId, savedMaqamDisplay, savedTonicDisplay, savedTonicEnglish, savedTonicSolfege,
             savedTransIdx, savedPresetIdx, savedStartMidi,
             savedDegreeNames, savedTonicChromatic, savedRefCents] ()
        {
            if (! isAlive (weak)) return;

            // Restore slider positions and cents offsets
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
            activeTuningState.perNoteCentsOverrides = savedPerNoteCents;

            // Restore maqam selection + display state (loadTuningSystem clears these)
            currentMaqamId          = savedMaqamId;
            currentMaqamDisplay     = savedMaqamDisplay;
            currentTonicDisplay     = savedTonicDisplay;
            currentTonicEnglish     = savedTonicEnglish;
            currentTonicSolfege     = savedTonicSolfege;
            currentTranspositionIdx = savedTransIdx;
            currentActivePresetIdx  = savedPresetIdx;
            currentStartMidi        = savedStartMidi;
            currentDegreeNames      = savedDegreeNames;
            currentTonicChromatic   = savedTonicChromatic;

            // Populate IPN/solfège from maqam list enriched degree data
            currentDegreeIpnRefs.fill ({});
            currentDegreeSolfegeRefs.fill ({});
            if (savedMaqamId.isNotEmpty())
            {
                for (const auto& mle : currentMaqamList)
                {
                    if (mle.maqamId == savedMaqamId)
                    {
                        if (savedTransIdx >= 0 && savedTransIdx < (int) mle.transpositions.size())
                            applyDegreeIpnAndSolfege (mle.transpositions[(size_t) savedTransIdx].degrees);
                        else
                            applyDegreeIpnAndSolfege (mle.degrees);
                        break;
                    }
                }
            }

            // Rebuild heptatonic map from restored maqam degrees
            rebuildHeptMap();

            // Restore reference frequency
            referenceCentsOffset = savedRefCents;
            if (refFreqParam != nullptr)
            {
                updatingParamsFromCode = true;
                refFreqParam->setValueNotifyingHost (
                    refFreqParam->getNormalisableRange().convertTo0to1 ((float) savedRefCents));
                updatingParamsFromCode = false;
            }

            // Sync APVTS params
            syncAllSlotParamsFromState();
            syncPresetParamFromState();

            updateTuningAndBroadcast();
            notifyTuningChanged();
        });
    }
}

// ── Tuning control ────────────────────────────────────────────────────────────

void TanghimProcessor::loadTuningSystem (const juce::String& systemId,
                                                    const juce::String& startingNote,
                                                    std::function<void()> onComplete)
{
    // Detect same-system reload (e.g. preset deactivation) — skip PAO name
    // matching so sliders reset to defaults (closest to 0 cents)
    const bool sameSystem = (systemId == currentSystemId
                             && startingNote == currentStartingNote);

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

    // Reset heptatonic map to identity (no maqam = no remapping)
    rebuildHeptMap();

    // Reference frequency offset is preserved across system/note switches —
    // user's concert pitch choice persists until explicitly changed.

    auto doLoad = [this, systemId, startingNote, onComplete, sameSystem] ()
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
        activeTuningState.clearPerNoteCentsOverrides();
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
        // Skip when reloading the same system (e.g. preset deactivation)
        // so sliders reset to defaults (closest to 0 cents).
        if (! sameSystem)
        {
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

        updateTuningAndBroadcast();
        if (onStatusMessage) onStatusMessage (buildScaleName());

        // Fetch maqam list BEFORE notifying UI, so preset compatibility
        // checks have the list available on the first render
        maqamDataLoading.store (true);
        fetchMaqamListIfNeeded();

        notifyTuningChanged();
        if (onComplete) onComplete();

        saveSettingsToDisk();

        // After the selected starting note loads, background-preload
        // all other starting notes for this tuning system
        backgroundPreloadRemainingNotes (systemId, startingNote);
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
            loadingFromCache.store (true);
            std::weak_ptr<std::atomic<bool>> weak (alive);
            apiClient.runOnThread ([this, weak, systemId, startingNote, doLoad]
            {
                dataCache.preload (systemId, startingNote);
                juce::MessageManager::callAsync ([this, weak, doLoad]
                {
                    if (! isAlive (weak)) return;
                    loadingFromCache.store (false);
                    doLoad();
                });
            });
        }
    }
    else
    {
        if (onStatusMessage) onStatusMessage ("Loading " + systemId + "…");
        downloadState.store (DownloadState::downloading);
        std::weak_ptr<std::atomic<bool>> weak (alive);
        apiClient.fetchPitchClasses (systemId, startingNote,
            [this, weak, systemId, startingNote, doLoad] (std::vector<PitchClass> pcs)
            {
                if (! isAlive (weak)) return;
                downloadState.store (DownloadState::idle);
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
            [this, weak, systemId, startingNote] (juce::String err)
            {
                if (! isAlive (weak)) return;
                downloadState.store (DownloadState::connectionError);
                lastFailedFetch = [this, systemId, startingNote] {
                    loadTuningSystem (systemId, startingNote);
                };
                if (onStatusMessage) onStatusMessage ("Error: " + err);
            });
    }
}

void TanghimProcessor::setSliderVariant (int chromaticIndex, int variantIndex)
{
    if (chromaticIndex < 0 || chromaticIndex >= 12) return;
    auto& slot = activeTuningState.slots[(size_t) chromaticIndex];
    slot.selectedIndex = juce::jlimit (0, slot.variantCount() - 1, variantIndex);

    // Sync centsOffset to the selected variant's deviation
    if (const auto* v = slot.selectedVariant())
        slot.centsOffset = v->midiCentsDeviation;

    // Clear per-note overrides for this chromatic position (all-octaves reset)
    for (int midi = chromaticIndex; midi < 128; midi += 12)
    {
        activeTuningState.perNoteVariantOverrides[(size_t) midi] = -1;
        activeTuningState.perNoteCentsOverrides[(size_t) midi] = std::numeric_limits<double>::quiet_NaN();
    }

    // Manual slider change deactivates preset (but preserves maqam association
    // so that degree highlights, hept map, and save/load work correctly)
    currentActivePresetIdx  = -1;

    // Sync APVTS params
    syncSlotParamFromState (chromaticIndex);
    syncPresetParamFromState();

    updateTuningAndBroadcast();
    notifyTuningChanged();
}

void TanghimProcessor::setSlotCents (int chromaticIndex, double centsValue)
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

    // Clear per-note cents overrides for this chromatic position
    // (chromatic slot drag overrides any per-note tuning)
    for (int midi = chromaticIndex; midi < 128; midi += 12)
        activeTuningState.perNoteCentsOverrides[(size_t) midi] = std::numeric_limits<double>::quiet_NaN();

    // Manual slider change deactivates preset (but preserves maqam association
    // so that degree highlights, hept map, and save/load work correctly)
    currentActivePresetIdx  = -1;

    // Sync APVTS param for DAW automation recording
    syncSlotParamFromState (chromaticIndex);

    // Update MTS-ESP immediately (live pitch change) — no WebView push
    updateTuningAndBroadcast();
}

void TanghimProcessor::finalizeSlotCents (int chromaticIndex, double centsValue)
{
    setSlotCents (chromaticIndex, centsValue);
    notifyTuningChanged();
}

void TanghimProcessor::setNoteCents (int midiNote, double centsValue)
{
    if (midiNote < 0 || midiNote >= 128) return;
    const double cents = juce::jlimit (-200.0, 200.0, centsValue);
    activeTuningState.perNoteCentsOverrides[(size_t) midiNote] = cents;

    // Update selectedIndex on the chromatic slot to nearest variant (for display)
    const int chromaticIdx = midiNote % 12;
    auto& slot = activeTuningState.slots[(size_t) chromaticIdx];
    int bestIdx = 0;
    double bestDist = std::numeric_limits<double>::max();
    for (int v = 0; v < slot.variantCount(); ++v)
    {
        const double dist = std::abs (slot.variants[(size_t) v].midiCentsDeviation - cents);
        if (dist < bestDist) { bestDist = dist; bestIdx = v; }
    }
    activeTuningState.perNoteVariantOverrides[(size_t) midiNote] = bestIdx;

    // Deactivate preset (preserve maqam)
    currentActivePresetIdx = -1;

    // Fast path: update just this one MIDI note in the tuning engine
    tuningEngine.updateNoteTuning (midiNote, cents, referenceCentsOffset);

    if (heptEnabled.load (std::memory_order_relaxed))
        rebroadcastHeptMts();
}

void TanghimProcessor::finalizeNoteCents (int midiNote, double centsValue)
{
    setNoteCents (midiNote, centsValue);
    syncPresetParamFromState();
    notifyTuningChanged();
}

void TanghimProcessor::setNoteVariant (int midiNote, int variantIndex)
{
    if (midiNote < 0 || midiNote >= 128) return;
    const int chromaticIdx = midiNote % 12;
    const auto& slot = activeTuningState.slots[(size_t) chromaticIdx];
    activeTuningState.perNoteVariantOverrides[(size_t) midiNote] =
        juce::jlimit (0, std::max (0, slot.variantCount() - 1), variantIndex);

    // Manual per-note override deactivates preset (but preserves maqam association
    // so that degree highlights, hept map, and save/load work correctly)
    currentActivePresetIdx  = -1;

    // Sync preset param only (per-note overrides don't affect slot params)
    syncPresetParamFromState();

    updateTuningAndBroadcast();
    notifyTuningChanged();
}

void TanghimProcessor::applyPreset (int idx)
{
    if (idx < 0 || idx >= kNumMaqamPresets) return;
    const auto& preset = presets[(size_t) idx];
    if (! preset.isAssigned) return;

    // Apply all slider positions and cents offsets directly (not via setSliderVariant which clears maqam state)
    activeTuningState.clearPerNoteOverrides();
    activeTuningState.clearPerNoteCentsOverrides();
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

    // Populate IPN/solfège from maqam list enriched degree data
    currentDegreeIpnRefs.fill ({});
    currentDegreeSolfegeRefs.fill ({});
    for (const auto& mle : currentMaqamList)
    {
        if (mle.maqamId == preset.maqamIdName)
        {
            if (preset.isTransposed && preset.pitchClassSetIndex >= 0
                && preset.pitchClassSetIndex < (int) mle.transpositions.size())
                applyDegreeIpnAndSolfege (mle.transpositions[(size_t) preset.pitchClassSetIndex].degrees);
            else
                applyDegreeIpnAndSolfege (mle.degrees);
            break;
        }
    }

    // Sync APVTS params
    syncAllSlotParamsFromState();
    syncPresetParamFromState();

    rebuildHeptMap();
    updateTuningAndBroadcast();
    notifyTuningChanged();
}

void TanghimProcessor::assignPreset (int idx,
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
    if (idx < 0 || idx >= kNumMaqamPresets) return;
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

void TanghimProcessor::applyMaqam (const juce::String& maqamId, int transpositionIndex)
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

        applyDegreeIpnAndSolfege (found->degrees);
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

        applyDegreeIpnAndSolfege (t.degrees);
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
}

void TanghimProcessor::applyDegreeIpnAndSolfege (const MaqamDegrees& degrees)
{
    currentDegreeIpnRefs.fill ({});
    currentDegreeSolfegeRefs.fill ({});

    for (size_t i = 0; i < degrees.ascendingEnglishNames.size(); ++i)
    {
        juce::String ipn = degrees.ascendingEnglishNames[i];
        if (ipn.isEmpty()) continue;

        // Strip trailing octave digits
        while (ipn.isNotEmpty() && juce::CharacterFunctions::isDigit (ipn.getLastCharacter()))
            ipn = ipn.dropLastCharacters (1);
        // Strip microtonal modifiers: "-b", "-#", etc. (indicated by "-" prefix)
        if (ipn.contains ("-"))
            ipn = ipn.upToFirstOccurrenceOf ("-", false, false);
        if (ipn.isEmpty()) continue;

        const int ci = chromaticIndexForIpnRef (ipn);
        if (ci >= 0)
        {
            currentDegreeIpnRefs[(size_t) ci] = ipn;
            if (i < degrees.ascendingSolfeges.size() && degrees.ascendingSolfeges[i].isNotEmpty())
                currentDegreeSolfegeRefs[(size_t) ci] = degrees.ascendingSolfeges[i];
        }
    }
}

void TanghimProcessor::clearPreset (int idx)
{
    if (idx < 0 || idx >= kNumMaqamPresets) return;
    presets[(size_t) idx].clear();

    if (currentActivePresetIdx == idx)
        currentActivePresetIdx = -1;

    savePresetsToDisk();
}

// ── Reference frequency control ───────────────────────────────────────────────

double TanghimProcessor::getReferenceDefaultHz() const
{
    return 440.0 * std::pow (2.0, (referenceNoteMidi - 69) / 12.0);
}

double TanghimProcessor::getReferenceCurrentHz() const
{
    return getReferenceDefaultHz() * std::pow (2.0, referenceCentsOffset / 1200.0);
}

void TanghimProcessor::setReferenceCentsOffset (double cents)
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

    // Fast path: only reapply the reference offset multiplier to the cached
    // base table. Avoids rebuilding the full 128-note table + cents table +
    // scale name on every drag tick.
    tuningEngine.updateReferenceOffset (referenceCentsOffset);

    if (heptEnabled.load (std::memory_order_relaxed))
        rebroadcastHeptMts();
}

void TanghimProcessor::finalizeReferenceCentsOffset (double cents)
{
    setReferenceCentsOffset (cents);
    notifyTuningChanged();
}

void TanghimProcessor::beginRefFreqGesture()
{
    if (refFreqParam != nullptr)
        refFreqParam->beginChangeGesture();
}

void TanghimProcessor::endRefFreqGesture()
{
    if (refFreqParam != nullptr)
        refFreqParam->endChangeGesture();
}

// ── Internal reference oscillator ──────────────────────────────────────────────

void TanghimProcessor::setOscillatorEnabled (bool enabled)
{
    const bool wasEnabled = oscillatorEnabled.exchange (enabled, std::memory_order_relaxed);
    if (wasEnabled && ! enabled)
        oscillator.allNotesOff();
}

void TanghimProcessor::setHeptEnabled (bool enabled)
{
    const bool wasEnabled = heptEnabled.exchange (enabled, std::memory_order_relaxed);
    if (wasEnabled && ! enabled)
    {
        // Notes were remapped — clean release needed
        oscillator.allNotesOff();
        for (int i = 0; i < 128; ++i)
            activeHeptNotes[i].store (-1, std::memory_order_relaxed);

        // Rebroadcast normal (non-remapped) table to MTS-ESP
        tuningEngine.rebroadcastCurrentTuning();
    }
    else if (! wasEnabled && enabled)
    {
        // Broadcast remapped table to MTS-ESP
        rebroadcastHeptMts();
    }
    saveSettingsToDisk();
}

void TanghimProcessor::rebuildHeptMap()
{
    // Map each degree to its natural white key using IPN letter name.
    // Bb→B key, Gb→G key, F#→F key, Eb→E key, etc.
    // This ensures musically correct mapping (Bb is a variant of B, not A).
    //
    // IPN source priority:
    //   1. currentDegreeIpnRefs[chromIdx] — context-aware from maqam detail
    //      (enharmonically correct: Saba uses Gb, Hijaz uses F# for same pitch)
    //   2. Pitch class ipnReference — from tuning system data (always available)

    struct DegreeInfo {
        int chromatic;     // 0-11 chromatic index
        int whiteKeyIdx;   // 0-6 index into whiteKeys[]
    };
    std::vector<DegreeInfo> degreeInfos;

    static constexpr int whiteKeys[] = { 0, 2, 4, 5, 7, 9, 11 };

    // Map IPN letter to white key index
    auto letterToWhiteIdx = [] (juce::juce_wchar letter) -> int {
        switch (letter) {
            case 'C': return 0; case 'D': return 1; case 'E': return 2;
            case 'F': return 3; case 'G': return 4; case 'A': return 5;
            case 'B': return 6; default: return -1;
        }
    };

    if (! currentDegreeNames.empty() && ! currentSystemId.isEmpty()
        && ! currentStartingNote.isEmpty()
        && dataCache.hasData (currentSystemId, currentStartingNote))
    {
        const auto& data = dataCache.getData (currentSystemId, currentStartingNote);

        // Build PAO name → chromatic index lookup
        std::map<juce::String, int> nameToChrom;
        for (const auto& pc : data.pitchClasses)
        {
            if (pc.noteName.isEmpty()) continue;
            const int ci = chromaticIndexForIpnRef (pc.ipnReference);
            if (ci >= 0 && nameToChrom.count (pc.noteName) == 0)
                nameToChrom[pc.noteName] = ci;
        }

        // Collect degree info: chromatic index + white key from IPN letter
        std::set<int> seenChroms;
        std::set<int> seenWhiteKeys;
        for (const auto& name : currentDegreeNames)
        {
            auto chromIt = nameToChrom.find (name);
            if (chromIt == nameToChrom.end()) continue;

            const int ci = chromIt->second;
            if (! seenChroms.insert (ci).second) continue;

            // Context-aware IPN ref from maqam detail (no fallback — tuning system IPN
            // may have wrong enharmonic, e.g. "C#" vs "Db", causing white key collision)
            juce::String ipn = currentDegreeIpnRefs[(size_t) ci];
            if (ipn.isEmpty()) continue;

            const int wki = letterToWhiteIdx (ipn[0]);
            if (wki < 0 || ! seenWhiteKeys.insert (wki).second) continue;

            degreeInfos.push_back ({ ci, wki });
            if (degreeInfos.size() >= 7) break;
        }
    }

    if (degreeInfos.empty())
    {
        // No degree info at all — identity map (delta 0 = no remapping)
        for (int i = 0; i < 12; ++i)
            heptMap[i].store (0, std::memory_order_relaxed);

        if (heptEnabled.load (std::memory_order_relaxed))
            rebroadcastHeptMts();
        return;
    }

    // ── Compute deltas ───────────────────────────────────────────────────────
    // Each white key gets a delta to reach its assigned degree chromatic index.
    // Black keys remain at delta 0 (chromatic passing tones).
    //
    // Example: Hijaz on D → degrees [D, Eb, F#, G, A, Bb, C]
    //   D(2)→D key Δ0, Eb(3)→E key(4) Δ-1, F#(6)→F key(5) Δ+1,
    //   G(7)→G key Δ0, A(9)→A key Δ0, Bb(10)→B key(11) Δ-1, C(0)→C key Δ0
    //
    // Example: maqam on Bb → Bb(10)→B key(11) Δ-1 (NOT A key)
    int deltas[12] = {};

    for (const auto& d : degreeInfos)
    {
        const int wk = whiteKeys[d.whiteKeyIdx];
        int delta = d.chromatic - wk;
        // Normalize to [-6, +5] — minimal semitone adjustment
        while (delta > 6)  delta -= 12;
        while (delta < -6) delta += 12;
        deltas[wk] = delta;
    }

    for (int i = 0; i < 12; ++i)
        heptMap[i].store (deltas[i], std::memory_order_relaxed);

    // If hept mode is active, re-broadcast remapped MTS-ESP table
    if (heptEnabled.load (std::memory_order_relaxed))
        rebroadcastHeptMts();
}

// ── Heptatonic MTS-ESP broadcast helpers ──────────────────────────────────────

void TanghimProcessor::updateTuningAndBroadcast()
{
    tuningEngine.updateTuning (activeTuningState, referenceCentsOffset, buildScaleName());

    // If hept mode is active, overwrite the MTS-ESP broadcast with remapped table
    if (heptEnabled.load (std::memory_order_relaxed))
        rebroadcastHeptMts();
}

void TanghimProcessor::rebroadcastHeptMts()
{
    // Build remapped table: for each MIDI note N, MTS-ESP freq[N] = internal freq[N + delta].
    // This makes MTS-ESP synths receiving the original (un-remapped) MIDI note play the
    // correct pitch. The internal freqTable is NOT modified — the oscillator already uses
    // remapped note numbers from the processBlock MIDI rewrite.
    const auto& internalTable = tuningEngine.getFrequencyTable();
    std::array<double, 128> remapped;

    for (int n = 0; n < 128; ++n)
    {
        const int delta = heptMap[n % 12].load (std::memory_order_relaxed);
        const int r = juce::jlimit (0, 127, n + delta);
        remapped[(size_t) n] = internalTable[(size_t) r];
    }

    tuningEngine.broadcastMtsTable (remapped);
}

// ── Accessors ─────────────────────────────────────────────────────────────────

const std::vector<TuningSystem>& TanghimProcessor::getTuningSystems() const
{
    return dataCache.getTuningSystemsList();
}

bool TanghimProcessor::hasCachedTuningData (const juce::String& systemId,
                                                      const juce::String& startingNote) const
{
    return dataCache.hasData (systemId, startingNote);
}

bool TanghimProcessor::hasTuningLoadedForCurrentSelection() const
{
    if (currentSystemId.isEmpty() || currentStartingNote.isEmpty())
        return false;
    // loadTuningSystem's doLoad fills every slot with variants from pitch-class data
    return activeTuningState.slots[0].variantCount() > 0;
}

bool TanghimProcessor::hasCachedMaqamList (const juce::String& systemId,
                                            const juce::String& startingNote) const
{
    return dataCache.hasMaqamList (systemId, startingNote);
}

void TanghimProcessor::retryLastFailedFetch()
{
    if (lastFailedFetch)
    {
        downloadState.store (DownloadState::idle);
        auto retry = std::move (lastFailedFetch);
        lastFailedFetch = nullptr;
        retry();
    }
}

const std::vector<MaqamListEntry>& TanghimProcessor::getMaqamList() const
{
    return currentMaqamList;
}

const ActiveTuningState& TanghimProcessor::getActiveTuningState() const
{
    return activeTuningState;
}

const std::array<MaqamPreset, kNumMaqamPresets>& TanghimProcessor::getPresets() const
{
    return presets;
}

juce::String TanghimProcessor::getCurrentSystemId()  const { return currentSystemId; }
juce::String TanghimProcessor::getCurrentStartingNote() const { return currentStartingNote; }

const std::vector<PitchClass>& TanghimProcessor::getCurrentPitchClasses() const
{
    static const std::vector<PitchClass> empty;
    if (currentSystemId.isEmpty() || currentStartingNote.isEmpty())
        return empty;
    if (! dataCache.hasData (currentSystemId, currentStartingNote))
        return empty;
    return dataCache.getData (currentSystemId, currentStartingNote).pitchClasses;
}

bool           TanghimProcessor::isMtsTransmitter()   const { return tuningEngine.isMtsTransmitter(); }
int            TanghimProcessor::mtsNumReceivers()   const { return tuningEngine.mtsNumReceivers(); }
ReceiverCounts TanghimProcessor::getReceiverCounts() const { return ReceiverRegistry::scan(); }

void TanghimProcessor::checkForDataUpdates (
    std::function<void (std::vector<juce::String>)> onUpdatesFound,
    std::function<void()>                            onNoUpdates,
    std::function<void (juce::String)>               onError)
{
    updateChecker.checkForUpdates (std::move (onUpdatesFound),
                                   std::move (onNoUpdates),
                                   std::move (onError));
}

bool TanghimProcessor::checkForDataUpdatesOnly (
    std::function<void (std::vector<juce::String>)> onStaleFound,
    std::function<void()>                            onAllCurrent,
    std::function<void (juce::String)>               onError)
{
    return updateChecker.checkOnly (
        [this, onStale = std::move (onStaleFound)] (std::vector<juce::String> staleIds) mutable
        {
            markAutoDataUpdateCheckCompleted();
            if (onStale) onStale (std::move (staleIds));
        },
        [this, onAll = std::move (onAllCurrent)] () mutable
        {
            markAutoDataUpdateCheckCompleted();
            if (onAll) onAll();
        },
        [this, onErr = std::move (onError)] (juce::String err) mutable
        {
            markAutoDataUpdateCheckCompleted();
            if (onErr) onErr (std::move (err));
        });
}

// ── Private helpers ───────────────────────────────────────────────────────────

void TanghimProcessor::fetchMaqamListIfNeeded()
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
            maqamDataLoading.store (false);
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
            maqamDataLoading.store (false);
            if (onMaqamListLoaded) onMaqamListLoaded();
        },
        [this, weak] (juce::String err)
        {
            if (! isAlive (weak)) return;
            DBG ("fetchMaqamListIfNeeded: API ERROR: " + err);
            maqamDataLoading.store (false);
            if (onStatusMessage) onStatusMessage ("Maqam list: " + err);
        });
}

void TanghimProcessor::backgroundPreloadRemainingNotes (const juce::String& systemId,
                                                         const juce::String& loadedNote)
{
    // Find all starting notes for this system
    std::vector<juce::String> otherNotes;
    for (const auto& sys : dataCache.getTuningSystemsList())
    {
        if (sys.id == systemId)
        {
            for (const auto& noteId : sys.startingNoteIds)
            {
                if (noteId != loadedNote)
                    otherNotes.push_back (noteId);
            }
            break;
        }
    }

    if (otherNotes.empty()) return;

    std::weak_ptr<std::atomic<bool>> weak (alive);

    // Phase 1 (concurrent): fetch pitch classes for all other starting notes
    auto pitchClassesRemaining = std::make_shared<std::atomic<int>> ((int) otherNotes.size());

    for (const auto& noteId : otherNotes)
    {
        if (dataCache.hasData (systemId, noteId))
        {
            // Already cached — decrement and check for phase 2
            if (pitchClassesRemaining->fetch_sub (1) == 1)
                backgroundPreloadMaqamLists (systemId, loadedNote, otherNotes);

            // Update cache icons
            juce::MessageManager::callAsync ([this, weak]
            {
                if (! isAlive (weak)) return;
                if (onTuningSystemsLoaded) onTuningSystemsLoaded();
            });
            continue;
        }

        apiClient.fetchPitchClasses (systemId, noteId,
            [this, systemId, noteId, weak, pitchClassesRemaining, loadedNote, otherNotes] (auto pitchClasses)
            {
                if (! isAlive (weak)) return;

                // Store with version from systems list
                ApiDataCache::TuningData td;
                td.pitchClasses = std::move (pitchClasses);
                for (const auto& ts : dataCache.getTuningSystemsList())
                    if (ts.id == systemId) { td.tuningSystemVersion = ts.version; break; }
                dataCache.storeData (systemId, noteId, std::move (td));

                // Update cache icons on message thread
                juce::MessageManager::callAsync ([this, weak]
                {
                    if (! isAlive (weak)) return;
                    if (onTuningSystemsLoaded) onTuningSystemsLoaded();
                });

                // When all pitch classes done → phase 2: maqam lists
                if (pitchClassesRemaining->fetch_sub (1) == 1)
                    backgroundPreloadMaqamLists (systemId, loadedNote, otherNotes);
            },
            [] (auto) {});  // Ignore individual errors during background preload
    }
}

void TanghimProcessor::backgroundPreloadMaqamLists (const juce::String& systemId,
                                                      const juce::String& loadedNote,
                                                      const std::vector<juce::String>& otherNotes)
{
    std::weak_ptr<std::atomic<bool>> weak (alive);

    for (const auto& noteId : otherNotes)
    {
        if (dataCache.hasMaqamList (systemId, noteId))
        {
            // Already has maqam list — update cache icons
            juce::MessageManager::callAsync ([this, weak]
            {
                if (! isAlive (weak)) return;
                if (onTuningSystemsLoaded) onTuningSystemsLoaded();
            });
            continue;
        }

        apiClient.fetchMaqamList (systemId, noteId,
            [this, systemId, noteId, weak] (auto maqamList)
            {
                if (! isAlive (weak)) return;
                dataCache.updateMaqamList (systemId, noteId, maqamList);

                juce::MessageManager::callAsync ([this, weak]
                {
                    if (! isAlive (weak)) return;
                    if (onTuningSystemsLoaded) onTuningSystemsLoaded();
                });
            },
            [] (auto) {});  // Ignore individual errors during background preload
    }
}

void TanghimProcessor::applyMaqamDegrees (const MaqamDegrees& degrees)
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
    activeTuningState.clearPerNoteCentsOverrides();
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

    // Rebuild heptatonic map from the new degree set
    rebuildHeptMap();

    // Sync APVTS params
    syncAllSlotParamsFromState();
    syncPresetParamFromState();

    updateTuningAndBroadcast();
    notifyTuningChanged();
}

void TanghimProcessor::rebuildTuningStateFromCache()
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

void TanghimProcessor::notifyTuningChanged()
{
    if (onTuningStateChanged) onTuningStateChanged();
}

juce::String TanghimProcessor::buildScaleName() const
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

void TanghimProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Skip if we're programmatically updating params to avoid feedback loops
    if (updatingParamsFromCode)
        return;

    // Session restore calls replaceState() before loadTuningSystem() finishes; preset/slot
    // listeners would run against stale tuning data and can nest loadTuningSystem().
    if (sessionRecallInProgress)
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
        {
            activeTuningState.perNoteVariantOverrides[(size_t) midi] = -1;
            activeTuningState.perNoteCentsOverrides[(size_t) midi] = std::numeric_limits<double>::quiet_NaN();
        }

        // NOTE: We intentionally do NOT clear maqam state here.
        // DAW automation should update tuning without breaking maqam association.
        // The explicit methods (setSliderVariant, setSlotCents) handle maqam clearing
        // for direct user interactions. Also, parameterChanged can be called
        // asynchronously from syncAllSlotParamsFromState, after updatingParamsFromCode
        // is reset, which would incorrectly clear maqam state.

        // Tuning update is handled by pollSlotAutomation() in processBlock at audio
        // buffer rate — much more responsive than this message-thread callback.
        // Just mark dirty for UI update at 30Hz.
        slotAutomationDirtyMask.fetch_or (uint16_t (1u << chromaticIdx),
                                          std::memory_order_relaxed);
    }
    // Handle reference frequency parameter
    // Fast path: reapply reference offset multiplier only (no full table rebuild).
    // UI update throttled to 30Hz via editor timer dirty flag — avoids calling
    // buildTuningStateJson() at MIDI CC rate (hundreds/sec) which causes CPU spikes.
    else if (parameterID == "ref_freq")
    {
        referenceCentsOffset = juce::jlimit (-700.0, 700.0, (double) newValue);
        tuningEngine.updateReferenceOffset (referenceCentsOffset);
        if (heptEnabled.load (std::memory_order_relaxed))
            rebroadcastHeptMts();
        refFreqAutomationDirty.store (true, std::memory_order_relaxed);
    }
    // Handle preset parameter
    else if (parameterID == "preset")
    {
        // newValue is normalized 0-1, use the parameter's getIndex() for actual choice
        if (presetParam == nullptr)
            return;

        const int choiceIdx = presetParam->getIndex(); // 0 = "None", 1-8 = presets 1-8
        const int presetIdx = choiceIdx - 1;           // -1 = "None", 0-7 = presets 1-8

        if (presetIdx >= 0 && presetIdx < kNumMaqamPresets)
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

void TanghimProcessor::syncSlotParamFromState (int chromaticIndex)
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

void TanghimProcessor::syncAllSlotParamsFromState()
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

void TanghimProcessor::syncPresetParamFromState()
{
    if (presetParam == nullptr)
        return;

    // currentActivePresetIdx is -1 for "None", 0-7 for presets 1-8
    // APVTS choice index: 0 = "None", 1-8 = presets 1-8
    const int choiceIdx = currentActivePresetIdx + 1;

    updatingParamsFromCode = true;
    presetParam->setValueNotifyingHost (
        presetParam->convertTo0to1 (static_cast<float> (juce::jlimit (0, kNumMaqamPresets, choiceIdx))));
    updatingParamsFromCode = false;
}

// ── Gesture marking ───────────────────────────────────────────────────────────

void TanghimProcessor::beginSliderGesture (int chromaticIndex)
{
    if (chromaticIndex < 0 || chromaticIndex >= kNumSlotParams)
        return;

    // Map chromatic index to tonic-relative slot index
    const int slotIdx = chromaticToSlot (chromaticIndex);
    auto* param = slotParams[(size_t) slotIdx];
    if (param != nullptr)
        param->beginChangeGesture();
}

void TanghimProcessor::endSliderGesture (int chromaticIndex)
{
    if (chromaticIndex < 0 || chromaticIndex >= kNumSlotParams)
        return;

    // Map chromatic index to tonic-relative slot index
    const int slotIdx = chromaticToSlot (chromaticIndex);
    auto* param = slotParams[(size_t) slotIdx];
    if (param != nullptr)
        param->endChangeGesture();
}

void TanghimProcessor::beginPresetGesture()
{
    if (presetParam != nullptr)
        presetParam->beginChangeGesture();
}

void TanghimProcessor::endPresetGesture()
{
    if (presetParam != nullptr)
        presetParam->endChangeGesture();
}

// ── Maqam/scroll state ───────────────────────────────────────────────────────

void TanghimProcessor::setStartMidi (double startMidi)
{
    currentStartMidi = startMidi;
}

void TanghimProcessor::setMidiPresetNote (int presetIdx, int midiNote)
{
    if (presetIdx >= 0 && presetIdx < kNumMaqamPresets)
    {
        midiPresetNotes[(size_t) presetIdx].store (juce::jlimit (-1, 127, midiNote), std::memory_order_relaxed);
        saveSettingsToDisk();
    }
}

int TanghimProcessor::getMidiPresetNote (int presetIdx) const
{
    if (presetIdx >= 0 && presetIdx < kNumMaqamPresets)
        return midiPresetNotes[(size_t) presetIdx].load (std::memory_order_relaxed);
    return -1;
}

void TanghimProcessor::setMidiPresetChannel (int channel)
{
    // 0 = any channel, 1-16 = specific channel
    midiPresetChannel.store (juce::jlimit (0, 16, channel), std::memory_order_relaxed);
    saveSettingsToDisk();
}

int TanghimProcessor::consumePendingMidiPreset()
{
    return pendingMidiPreset.exchange (-1, std::memory_order_relaxed);
}

void TanghimProcessor::startMidiLearn (int presetIdx)
{
    if (presetIdx >= 0 && presetIdx < kNumMaqamPresets)
    {
        midiLearnTargetPreset.store (presetIdx, std::memory_order_relaxed);
        notifyTuningChanged();  // Update UI to show learning animation immediately
    }
}

void TanghimProcessor::cancelMidiLearn()
{
    midiLearnTargetPreset.store (-1, std::memory_order_relaxed);
    notifyTuningChanged();  // Update UI to hide learning animation
}

void TanghimProcessor::clearMidiPresetNote (int presetIdx)
{
    if (presetIdx >= 0 && presetIdx < kNumMaqamPresets)
    {
        midiPresetNotes[(size_t) presetIdx].store (-1, std::memory_order_relaxed);
        saveSettingsToDisk();
        notifyTuningChanged();  // Update UI to remove MIDI badge immediately
    }
}

void TanghimProcessor::clearAllMidiPresetNotes()
{
    for (int i = 0; i < kNumMaqamPresets; ++i)
        midiPresetNotes[(size_t) i].store (-1, std::memory_order_relaxed);
    saveSettingsToDisk();
}

// ── Direct MIDI device input ─────────────────────────────────────────────────

juce::StringArray TanghimProcessor::getAvailableMidiDevices() const
{
    juce::StringArray devices;
    devices.add ("None");  // First option to disable
    for (const auto& info : juce::MidiInput::getAvailableDevices())
        devices.add (info.name);
    return devices;
}

juce::String TanghimProcessor::getMidiPresetDevice() const
{
    return midiPresetDeviceName;
}

bool TanghimProcessor::isMidiPresetDeviceOpen() const
{
    return midiPresetInput != nullptr;
}

void TanghimProcessor::setMidiPresetDevice (const juce::String& deviceName)
{
    DBG ("setMidiPresetDevice: requested=\"" + deviceName
         + "\" current=\"" + midiPresetDeviceName
         + "\" open=" + juce::String (midiPresetInput != nullptr ? "yes" : "no"));

    // Close existing input if any
    if (midiPresetInput)
    {
        midiPresetInput->stop();
        midiPresetInput.reset();
        DBG ("  closed previous device");
    }

    midiPresetDeviceName = deviceName;
    midiPresetDeviceId.clear();

    if (deviceName.isEmpty() || deviceName == "None")
        return;

    // Try matching by saved identifier first (more reliable than name)
    const auto availableDevices = juce::MidiInput::getAvailableDevices();

    // Try identifier match first (handles reconnection of same physical device)
    if (midiPresetDeviceId.isNotEmpty())
    {
        for (const auto& info : availableDevices)
        {
            if (info.identifier == midiPresetDeviceId)
            {
                midiPresetInput = juce::MidiInput::openDevice (info.identifier, this);
                if (midiPresetInput)
                {
                    midiPresetInput->start();
                    midiPresetDeviceName = info.name;  // Update name in case it changed
                    DBG ("  opened by identifier: " + info.name + " [" + info.identifier + "]");
                    return;
                }
            }
        }
    }

    // Fall back to name match
    for (const auto& info : availableDevices)
    {
        if (info.name == deviceName)
        {
            midiPresetInput = juce::MidiInput::openDevice (info.identifier, this);
            if (midiPresetInput)
            {
                midiPresetInput->start();
                midiPresetDeviceId = info.identifier;
                DBG ("  opened by name: " + deviceName + " [" + info.identifier + "]");
            }
            else
            {
                DBG ("  FAILED to open: " + deviceName + " [" + info.identifier + "]");
            }
            return;
        }
    }

    DBG ("  device not found in available list (" + juce::String (availableDevices.size()) + " devices)");
}

void TanghimProcessor::recheckMidiPresetDevice()
{
    // Nothing to check if no device is configured or no connection exists
    if (midiPresetDeviceName.isEmpty() || midiPresetInput == nullptr)
        return;

    // Verify the device identifier is still in the available list
    bool found = false;
    for (const auto& info : juce::MidiInput::getAvailableDevices())
    {
        if (info.identifier == midiPresetDeviceId || info.name == midiPresetDeviceName)
        {
            found = true;
            break;
        }
    }

    if (! found)
    {
        DBG ("recheckMidiPresetDevice: device \"" + midiPresetDeviceName + "\" gone — closing stale connection");
        midiPresetInput->stop();
        midiPresetInput.reset();
        // Keep midiPresetDeviceName so reconnection can happen via timer
    }
}

void TanghimProcessor::handleIncomingMidiMessage (juce::MidiInput* /*source*/,
                                                            const juce::MidiMessage& message)
{
    if (! message.isNoteOn())
        return;

    const int channel  = midiPresetChannel.load (std::memory_order_relaxed);
    const int note     = message.getNoteNumber();
    const int midiCh   = message.getChannel();  // 1-16

    DBG ("handleIncomingMidiMessage: note=" + juce::String (note)
         + " ch=" + juce::String (midiCh)
         + " filter=" + juce::String (channel)
         + " learn=" + juce::String (midiLearnTargetPreset.load()));

    // Check channel filter first
    if (channel != 0 && midiCh != channel)
        return;

    // Check if we're in MIDI learn mode
    const int learnTarget = midiLearnTargetPreset.load (std::memory_order_relaxed);
    if (learnTarget >= 0 && learnTarget < kNumMaqamPresets)
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
    for (int i = 0; i < kNumMaqamPresets; ++i)
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

void TanghimProcessor::saveSettingsToDisk() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("tuningSystemId", currentSystemId);
    obj->setProperty ("startingNote",   currentStartingNote);

    // Save MIDI preset mappings
    juce::Array<juce::var> midiNotesArr;
    for (int i = 0; i < kNumMaqamPresets; ++i)
        midiNotesArr.add (juce::var (midiPresetNotes[(size_t) i].load (std::memory_order_relaxed)));
    obj->setProperty ("midiPresetNotes", midiNotesArr);
    obj->setProperty ("midiPresetChannel", midiPresetChannel.load (std::memory_order_relaxed));
    obj->setProperty ("midiPresetDevice", midiPresetDeviceName);
    obj->setProperty ("midiPresetDeviceId", midiPresetDeviceId);
    obj->setProperty ("oscillatorEnabled", oscillatorEnabled.load (std::memory_order_relaxed));
    obj->setProperty ("heptEnabled", heptEnabled.load (std::memory_order_relaxed));

    const auto dir = getTanghimDir();
    dir.createDirectory();
    dir.getChildFile ("settings.json")
       .replaceWithText (juce::JSON::toString (juce::var (obj)));
}

void TanghimProcessor::clearCache()
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
    rebuildHeptMap();

    // NOTE: presets are NOT cleared — they are user data, not cache
    currentActivePresetIdx = -1;

    // Reset slider state
    activeTuningState.clearPerNoteOverrides();
    activeTuningState.clearPerNoteCentsOverrides();
    for (int i = 0; i < 12; ++i)
    {
        auto& slot = activeTuningState.slots[(size_t) i];
        slot.variants.clear();
        slot.selectedIndex = 0;
        slot.centsOffset   = 0.0;
    }

    // Update MTS-ESP tuning table and notify UI
    updateTuningAndBroadcast();
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

void TanghimProcessor::loadSettingsFromDisk()
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
            for (int i = 0; i < juce::jmin (kNumMaqamPresets, arr->size()); ++i)
                midiPresetNotes[(size_t) i].store (static_cast<int> ((*arr)[i]), std::memory_order_relaxed);
        }

        auto channelProp = obj->getProperty ("midiPresetChannel");
        if (! channelProp.isVoid())
            midiPresetChannel.store (static_cast<int> (channelProp), std::memory_order_relaxed);

        auto deviceIdProp = obj->getProperty ("midiPresetDeviceId");
        if (! deviceIdProp.isVoid())
            midiPresetDeviceId = deviceIdProp.toString();

        auto deviceProp = obj->getProperty ("midiPresetDevice");
        if (! deviceProp.isVoid())
        {
            const auto savedDevice = deviceProp.toString();
            setMidiPresetDevice (savedDevice);
        }

        auto oscEnabledProp = obj->getProperty ("oscillatorEnabled");
        if (! oscEnabledProp.isVoid())
            oscillatorEnabled.store ((bool) oscEnabledProp, std::memory_order_relaxed);

        auto heptEnabledProp = obj->getProperty ("heptEnabled");
        if (! heptEnabledProp.isVoid())
            heptEnabled.store ((bool) heptEnabledProp, std::memory_order_relaxed);
    }
}

void TanghimProcessor::savePresetsToDisk() const
{
    juce::Array<juce::var> arr;
    for (int i = 0; i < kNumMaqamPresets; ++i)
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

void TanghimProcessor::loadPresetsFromDisk()
{
    const auto file = getTanghimDir().getChildFile ("presets.json");
    if (! file.existsAsFile()) return;

    auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (auto* arr = parsed.getArray())
    {
        for (int i = 0; i < juce::jmin (kNumMaqamPresets, arr->size()); ++i)
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
    return new TanghimProcessor();
}
