#pragma once
#include <cmath>
#include <array>

/**
 * Lightweight polyphonic triangle-wave oscillator for reference pitch audition.
 *
 * Design:
 *   - 16 pre-allocated voices (no audio-thread allocation)
 *   - Naive (non-band-limited) triangle — fine for a reference oscillator
 *   - Linear attack (5 ms) and release (100 ms) envelope to avoid clicks
 *   - Frequency can be updated per-buffer for live slider drag feedback
 */

struct OscVoice
{
    bool   active      = false;
    bool   releasing   = false;
    int    midiNote    = -1;
    double phase       = 0.0;
    double phaseInc    = 0.0;
    double envelope    = 0.0;
    double attackStep  = 0.0;   // per-sample increment during attack
    double releaseStep = 0.0;   // per-sample decrement during release

    void noteOn (int note, double freqHz, double sampleRate)
    {
        midiNote    = note;
        active      = true;
        releasing   = false;
        phase       = 0.0;
        envelope    = 0.0;
        phaseInc    = freqHz / sampleRate;
        attackStep  = 1.0 / (0.005 * sampleRate);   // 5 ms attack
        releaseStep = 1.0 / (0.100 * sampleRate);   // 100 ms release
    }

    void noteOff()
    {
        if (active)
            releasing = true;
    }

    void updateFrequency (double freqHz, double sampleRate)
    {
        phaseInc = freqHz / sampleRate;
    }

    double nextSample()
    {
        if (! active) return 0.0;

        // Triangle: ramp from -1 to +1 and back using abs
        const double tri = 4.0 * std::abs (phase - 0.5) - 1.0;

        // Advance phase
        phase += phaseInc;
        if (phase >= 1.0) phase -= 1.0;

        // Envelope
        if (releasing)
        {
            envelope -= releaseStep;
            if (envelope <= 0.0)
            {
                envelope = 0.0;
                active   = false;
                midiNote = -1;
                return 0.0;
            }
        }
        else
        {
            if (envelope < 1.0)
            {
                envelope += attackStep;
                if (envelope > 1.0) envelope = 1.0;
            }
        }

        return tri * envelope;
    }
};

struct TriangleOscillator
{
    static constexpr int kMaxVoices = 16;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
    }

    void noteOn (int midiNote, double freqHz)
    {
        // Re-trigger: if this note is already playing, reuse that voice
        for (auto& v : voices)
        {
            if (v.active && v.midiNote == midiNote)
            {
                v.noteOn (midiNote, freqHz, sampleRate);
                return;
            }
        }

        // Find a free voice
        for (auto& v : voices)
        {
            if (! v.active)
            {
                v.noteOn (midiNote, freqHz, sampleRate);
                return;
            }
        }

        // Voice stealing: round-robin
        voices[(size_t) nextStealIdx].noteOn (midiNote, freqHz, sampleRate);
        nextStealIdx = (nextStealIdx + 1) % kMaxVoices;
    }

    void noteOff (int midiNote)
    {
        // Release the first active voice matching this note
        for (auto& v : voices)
        {
            if (v.active && ! v.releasing && v.midiNote == midiNote)
            {
                v.noteOff();
                return;
            }
        }
    }

    double processSample()
    {
        double sum = 0.0;
        for (auto& v : voices)
            sum += v.nextSample();
        return sum;
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.active)
                v.noteOff();
    }

    std::array<OscVoice, kMaxVoices> voices {};
    double sampleRate    = 44100.0;
    int    nextStealIdx  = 0;
};
