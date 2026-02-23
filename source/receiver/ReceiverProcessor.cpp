#include "ReceiverProcessor.h"
#include "ReceiverEditor.h"
#include "libMTSClient.h"
#include <cmath>

ReceiverProcessor::ReceiverProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    modeParam       = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("mode"));
    mpePbRangeParam  = dynamic_cast<juce::AudioParameterInt*>   (apvts.getParameter ("mpePbRange"));
    monoPbRangeParam = dynamic_cast<juce::AudioParameterInt*>   (apvts.getParameter ("monoPbRange"));

    jassert (modeParam != nullptr);
    jassert (mpePbRangeParam != nullptr);
    jassert (monoPbRangeParam != nullptr);

    for (int i = 0; i < 128; ++i)
        centsParams[(size_t) i] = dynamic_cast<juce::AudioParameterFloat*> (
            apvts.getParameter ("cents_" + juce::String (i)));

    lastCentsValues.fill (0.0f);

    mtsClient = MTS_RegisterClient();
    DBG ("ReceiverProcessor: MTS-ESP client registered");

    // Register with file-based registry for Transmitter discovery
    registryIsMpe = (modeParam->getIndex() == 0);
    registryUuid = ReceiverRegistry::announce (registryIsMpe);
    apvts.addParameterListener ("mode", this);
    startTimerHz (1);
}

ReceiverProcessor::~ReceiverProcessor()
{
    stopTimer();
    apvts.removeParameterListener ("mode", this);
    ReceiverRegistry::deannounce (registryUuid, registryIsMpe);

    if (mtsClient != nullptr)
        MTS_DeregisterClient (mtsClient);
}

juce::AudioProcessorValueTreeState::ParameterLayout ReceiverProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID ("mode", 1), "Mode",
        juce::StringArray { "MPE", "Pitch Bend" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID ("mpePbRange", 1), "MPE PB Range",
        1, 96, 48));

    params.push_back (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID ("monoPbRange", 1), "Mono PB Range",
        2, 96, 2));

    // 128 cents-deviation parameters for Max/M4L parameter bridge
    for (int i = 0; i < 128; ++i)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("cents_" + juce::String (i), 1),
            "Cents " + juce::String (i),
            juce::NormalisableRange<float> (-4800.0f, 4800.0f, 0.01f),
            0.0f));
    }

    return { params.begin(), params.end() };
}

void ReceiverProcessor::prepareToPlay (double, int)
{
    mpeSentZoneConfig = false;
}

void ReceiverProcessor::processBlock (juce::AudioBuffer<float>& audio,
                                      juce::MidiBuffer& midi)
{
    audio.clear();

    // Update connection status (read by editor on message thread)
    const bool hasMaster = (mtsClient != nullptr && MTS_HasMaster (mtsClient));
    connectedToMaster.store (hasMaster, std::memory_order_relaxed);

    if (hasMaster)
    {
        const char* name = MTS_GetScaleName (mtsClient);
        const juce::SpinLock::ScopedLockType lock (scaleNameLock);
        currentScaleName = (name != nullptr && name[0] != '\0')
                               ? juce::String::fromUTF8 (name) : "unnamed";
    }

    if (! hasMaster)
        return; // pass MIDI through unchanged

    // Sync pitch bend ranges from parameters
    mpeProcessor.setPitchBendRange (mpePbRangeParam->get());
    monoProcessor.setPitchBendRange (monoPbRangeParam->get());

    const bool isMpe = (modeParam->getIndex() == 0);

    // Mode switch: flush active notes from the OLD processor
    if (isMpe != lastWasMpe)
    {
        juce::MidiBuffer modeSwitchOutput;
        if (lastWasMpe)
            mpeProcessor.allNotesOff (modeSwitchOutput);
        else
            monoProcessor.allNotesOff (modeSwitchOutput);
        lastWasMpe = isMpe;
        midi.swapWith (modeSwitchOutput);
        return;
    }

    // Skip heavy work when no MIDI is present and no notes are sounding
    const bool hasActiveMpeNotes  = mpeProcessor.hasActiveNotes();
    const bool hasActiveMonoNotes = monoProcessor.hasActiveNotes();
    if (midi.isEmpty() && ! hasActiveMpeNotes && ! hasActiveMonoNotes)
    {
        // Still update cents params periodically for M4L bridge
        if (++paramUpdateCounter >= 50) // ~once per second at 48kHz/512
        {
            paramUpdateCounter = 0;
            for (int i = 0; i < 128; ++i)
            {
                double semitones = MTS_RetuningInSemitones (mtsClient, static_cast<char> (i), -1);
                if (std::isnan (semitones) || std::isinf (semitones))
                    semitones = 0.0;
                const float cents = static_cast<float> (semitones * 100.0);
                if (std::abs (cents - lastCentsValues[(size_t) i]) > 0.01f)
                {
                    lastCentsValues[(size_t) i] = cents;
                    if (centsParams[(size_t) i] != nullptr)
                        centsParams[(size_t) i]->setValueNotifyingHost (
                            centsParams[(size_t) i]->convertTo0to1 (cents));
                }
            }
        }
        return;
    }

    // Build cents deviation table from MTS-ESP transmitter
    std::array<double, 128> centsTable {};
    for (int i = 0; i < 128; ++i)
    {
        double semitones = MTS_RetuningInSemitones (mtsClient, static_cast<char> (i), -1);
        if (std::isnan (semitones) || std::isinf (semitones))
            semitones = 0.0;
        centsTable[static_cast<size_t> (i)] = semitones * 100.0;
    }

    // Update cents parameters for Max/M4L bridge (rate-limited)
    if (++paramUpdateCounter >= 2)
    {
        paramUpdateCounter = 0;
        for (int i = 0; i < 128; ++i)
        {
            const float cents = static_cast<float> (centsTable[(size_t) i]);
            if (std::abs (cents - lastCentsValues[(size_t) i]) > 0.01f)
            {
                lastCentsValues[(size_t) i] = cents;
                if (centsParams[(size_t) i] != nullptr)
                    centsParams[(size_t) i]->setValueNotifyingHost (
                        centsParams[(size_t) i]->convertTo0to1 (cents));
            }
        }
    }

    // Filter out notes the transmitter has marked as unmapped
    juce::MidiBuffer filtered;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            if (MTS_ShouldFilterNote (mtsClient, static_cast<char> (msg.getNoteNumber()), -1))
                continue; // skip filtered Note On
        }
        filtered.addEvent (msg, meta.samplePosition);
    }

    juce::MidiBuffer output;

    if (isMpe)
    {
        if (! mpeSentZoneConfig)
        {
            mpeProcessor.sendMpeZoneConfig (output);
            mpeSentZoneConfig = true;
        }
        mpeProcessor.process (filtered, output, centsTable, audio.getNumSamples());
    }
    else
    {
        monoProcessor.process (filtered, output, centsTable);
    }

    midi.swapWith (output);
}

juce::AudioProcessorEditor* ReceiverProcessor::createEditor()
{
    return new ReceiverEditor (*this);
}

void ReceiverProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ReceiverProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// ── Registry heartbeat + mode change ──────────────────────────────────────────

void ReceiverProcessor::timerCallback()
{
    ReceiverRegistry::heartbeat (registryUuid, registryIsMpe);
}

void ReceiverProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "mode")
    {
        const bool newIsMpe = (static_cast<int> (newValue) == 0);
        if (newIsMpe != registryIsMpe)
        {
            ReceiverRegistry::switchMode (registryUuid, registryIsMpe, newIsMpe);
            registryIsMpe = newIsMpe;
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReceiverProcessor();
}
