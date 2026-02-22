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

    mtsEsp = std::make_unique<MtsEspTransmitter>();
}

TuningEngine::~TuningEngine() = default;

// ── MTS-ESP status ───────────────────────────────────────────────────────────

bool TuningEngine::isMtsTransmitter()  const { return mtsEsp && mtsEsp->isTransmitter(); }
int  TuningEngine::mtsNumReceivers()   const { return mtsEsp ? mtsEsp->numReceivers() : 0; }

// ── Tuning update ─────────────────────────────────────────────────────────────

void TuningEngine::updateTuning (const ActiveTuningState& state,
                                  double referenceCentsOffset,
                                  const juce::String& scaleName)
{
    auto newFreqs  = state.buildFrequencyTable();
    const auto newCents  = state.buildCentsDeviationTable();

    // Apply global reference frequency offset (concert pitch shift)
    if (referenceCentsOffset != 0.0)
    {
        const double ratio = std::pow (2.0, referenceCentsOffset / 1200.0);
        for (auto& f : newFreqs)
            f *= ratio;
    }

    {
        juce::ScopedLock sl (tuningLock);
        freqTable  = newFreqs;
        centsTable = newCents;
    }

    // Broadcast via MTS-ESP so Receiver plugins can read the tuning
    if (mtsEsp && mtsEsp->isTransmitter())
    {
        mtsEsp->setTuningTable (newFreqs);
        if (scaleName.isNotEmpty())
            mtsEsp->setScaleName (scaleName);
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
    return freqTable;
}

const std::array<double, 128>& TuningEngine::getCentsDeviationTable() const
{
    return centsTable;
}
