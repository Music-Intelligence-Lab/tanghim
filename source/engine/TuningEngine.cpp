#include "TuningEngine.h"
#include <cmath>

TuningEngine::TuningEngine()
{
    // Initialise tables to 12-EDO
    for (int i = 0; i < 128; ++i)
    {
        const double freq = 440.0 * std::pow (2.0, (i - 69) / 12.0);
        freqTable    [(size_t)i] = freq;
        baseFreqTable[(size_t)i] = freq;
        centsTable   [(size_t)i] = 0.0;
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

    // Cache the base table (before reference offset) for fast ref-only updates
    auto baseFreqs = newFreqs;

    // Apply global reference frequency offset (concert pitch shift)
    if (referenceCentsOffset != 0.0)
    {
        const double ratio = std::pow (2.0, referenceCentsOffset / 1200.0);
        for (auto& f : newFreqs)
            f *= ratio;
    }

    {
        juce::ScopedLock sl (tuningLock);
        baseFreqTable = baseFreqs;
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

void TuningEngine::updateReferenceOffset (double referenceCentsOffset)
{
    std::array<double, 128> newFreqs;

    {
        juce::ScopedLock sl (tuningLock);
        newFreqs = baseFreqTable;
    }

    if (referenceCentsOffset != 0.0)
    {
        const double ratio = std::pow (2.0, referenceCentsOffset / 1200.0);
        for (auto& f : newFreqs)
            f *= ratio;
    }

    {
        juce::ScopedLock sl (tuningLock);
        freqTable = newFreqs;
        // centsTable unchanged — reference offset doesn't affect cents display
    }

    if (mtsEsp && mtsEsp->isTransmitter())
        mtsEsp->setTuningTable (newFreqs);
}

void TuningEngine::updateSlotTuning (int chromaticIndex, double centsOffset,
                                     double referenceCentsOffset)
{
    const double refRatio = (referenceCentsOffset != 0.0)
        ? std::pow (2.0, referenceCentsOffset / 1200.0)
        : 1.0;

    std::array<double, 128> newFreqs;

    {
        juce::ScopedLock sl (tuningLock);

        // Update only the ~11 MIDI notes that share this chromatic position
        for (int midi = chromaticIndex; midi < 128; midi += 12)
        {
            double baseFreq;
            if (centsOffset != 0.0)
            {
                const double centsFromA440 = (midi - 69) * 100.0 + centsOffset;
                baseFreq = 440.0 * std::pow (2.0, centsFromA440 / 1200.0);
            }
            else
            {
                baseFreq = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            }

            baseFreqTable[(size_t) midi] = baseFreq;
            freqTable[(size_t) midi]     = baseFreq * refRatio;
            centsTable[(size_t) midi]    = centsOffset;
        }

        newFreqs = freqTable;
    }

    if (mtsEsp && mtsEsp->isTransmitter())
        mtsEsp->setTuningTable (newFreqs);
}

// ── MTS-ESP broadcast helpers ─────────────────────────────────────────────────

void TuningEngine::broadcastMtsTable (const std::array<double, 128>& freqs)
{
    if (mtsEsp && mtsEsp->isTransmitter())
        mtsEsp->setTuningTable (freqs);
}

void TuningEngine::rebroadcastCurrentTuning()
{
    juce::ScopedLock sl (tuningLock);
    broadcastMtsTable (freqTable);
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

void TuningEngine::snapshotFrequencyTable (std::array<double, 128>& dest) const
{
    juce::ScopedLock sl (tuningLock);
    dest = freqTable;
}
