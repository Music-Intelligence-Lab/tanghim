#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/MpePitchBendProcessor.h"
#include "engine/MonoPitchBendProcessor.h"
#include "receiver/ReceiverRegistry.h"

struct MTSClient;

class ReceiverProcessor : public juce::AudioProcessor,
                          private juce::Timer,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    ReceiverProcessor();
    ~ReceiverProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool   acceptsMidi()  const override { return true; }
    bool   producesMidi() const override { return true; }
    bool   isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Thread-safe status for the editor (updated from audio thread only)
    bool isConnectedToMaster() const { return connectedToMaster.load (std::memory_order_relaxed); }
    juce::String getScaleName() const
    {
        const juce::SpinLock::ScopedLockType lock (scaleNameLock);
        return currentScaleName;
    }

    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    MTSClient* mtsClient = nullptr;

    MpePitchBendProcessor  mpeProcessor;
    MonoPitchBendProcessor monoProcessor;

    juce::AudioParameterChoice* modeParam       = nullptr;
    juce::AudioParameterInt*    mpePbRangeParam  = nullptr;
    juce::AudioParameterInt*    monoPbRangeParam = nullptr;

    // 128 cents-deviation parameters for Max/M4L parameter bridge
    std::array<juce::AudioParameterFloat*, 128> centsParams {};
    std::array<float, 128> lastCentsValues {};
    int paramUpdateCounter = 0;

    bool mpeSentZoneConfig = false;
    bool lastWasMpe = true;  // tracks mode across processBlock calls for Note Off flush

    // Status updated in processBlock, read by editor
    std::atomic<bool>    connectedToMaster { false };
    mutable juce::SpinLock scaleNameLock;
    juce::String         currentScaleName;

    // ── File-based registry for Transmitter discovery ─────────────────────
    juce::String registryUuid;
    bool registryIsMpe = true;

    void timerCallback() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReceiverProcessor)
};
