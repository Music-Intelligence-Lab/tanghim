#include "TuningEngine.h"
#include <cmath>

TuningEngine::TuningEngine()
{
    // Initialise tables to 12-EDO
    for (int i = 0; i < 128; ++i)
    {
        freqTable [(size_t)i] = 440.0 * std::pow (2.0, (i - 69) / 12.0);
        centsTable[(size_t)i] = 0.0;
    }

    mtsEsp  = std::make_unique<MtsEspTransmitter>();
    mpe     = std::make_unique<MpePitchBendProcessor> (48);
    monoPb  = std::make_unique<MonoPitchBendProcessor> (2);
}

TuningEngine::~TuningEngine() = default;

// ── Output mode ───────────────────────────────────────────────────────────────

void TuningEngine::setOutputMode (OutputMode mode)
{
    currentMode = mode;
    mpeZoneConfigSent = false; // force resend on next block

    // Always keep MTS-ESP in sync so Receiver plugins work in any mode
    pushCurrentTuningToMts();
}

OutputMode TuningEngine::getOutputMode() const { return currentMode; }

bool TuningEngine::isMtsTransmitter()  const { return mtsEsp && mtsEsp->isTransmitter(); }
int  TuningEngine::mtsNumReceivers()   const { return mtsEsp ? mtsEsp->numReceivers() : 0; }

void TuningEngine::setMpePitchBendRange (int semitones)
{
    if (mpe) mpe->setPitchBendRange (semitones);
}

void TuningEngine::setMonoPitchBendRange (int semitones)
{
    if (monoPb) monoPb->setPitchBendRange (semitones);
}

int TuningEngine::getMpePitchBendRange() const  { return mpe    ? mpe->getPitchBendRange()    : 48; }
int TuningEngine::getMonoPitchBendRange() const { return monoPb ? monoPb->getPitchBendRange() : 2; }

// ── Tuning update ─────────────────────────────────────────────────────────────

void TuningEngine::updateTuning (const ActiveTuningState& state,
                                  const juce::String& scaleName)
{
    const auto newFreqs  = state.buildFrequencyTable();
    const auto newCents  = state.buildCentsDeviationTable();

    {
        juce::ScopedLock sl (tuningLock);
        freqTable  = newFreqs;
        centsTable = newCents;
    }

    // Always broadcast via MTS-ESP so Receiver plugins can read the tuning,
    // regardless of which output mode the Transmitter is using for its own MIDI.
    if (mtsEsp && mtsEsp->isTransmitter())
    {
        mtsEsp->setTuningTable (newFreqs);
        if (scaleName.isNotEmpty())
            mtsEsp->setScaleName (scaleName);
    }
}

void TuningEngine::pushCurrentTuningToMts()
{
    if (mtsEsp && mtsEsp->isTransmitter())
    {
        juce::ScopedLock sl (tuningLock);
        mtsEsp->setTuningTable (freqTable);
    }
}

// ── Audio-thread processing ───────────────────────────────────────────────────

void TuningEngine::processMidi (juce::MidiBuffer& midiInOut, int numSamples)
{
    switch (currentMode)
    {
        case OutputMode::MtsEsp:
            // Tuning is applied out-of-band via shared memory.
            // MIDI passes through completely unchanged.
            break;

        case OutputMode::Mpe:
        {
            juce::MidiBuffer processed;
            if (! mpeZoneConfigSent)
            {
                mpe->sendMpeZoneConfig (processed, 0);
                mpeZoneConfigSent = true;
            }
            std::array<double, 128> cents;
            {
                juce::ScopedLock sl (tuningLock);
                cents = centsTable;
            }
            mpe->process (midiInOut, processed, cents, numSamples);
            midiInOut.swapWith (processed);
            break;
        }

        case OutputMode::MonoPitchBend:
        {
            juce::MidiBuffer processed;
            std::array<double, 128> cents;
            {
                juce::ScopedLock sl (tuningLock);
                cents = centsTable;
            }
            monoPb->process (midiInOut, processed, cents);
            midiInOut.swapWith (processed);
            break;
        }
    }
}

// ── Accessors ─────────────────────────────────────────────────────────────────

double TuningEngine::getFrequencyForMidiNote (int midi) const
{
    juce::ScopedLock sl (tuningLock);
    if (midi < 0 || midi >= 128) return 0.0;
    return freqTable[(size_t) midi];
}

double TuningEngine::getCentsDeviationForMidi (int midi) const
{
    juce::ScopedLock sl (tuningLock);
    if (midi < 0 || midi >= 128) return 0.0;
    return centsTable[(size_t) midi];
}

const std::array<double, 128>& TuningEngine::getFrequencyTable() const
{
    return freqTable; // caller must hold lock or be on message thread
}

const std::array<double, 128>& TuningEngine::getCentsDeviationTable() const
{
    return centsTable;
}
