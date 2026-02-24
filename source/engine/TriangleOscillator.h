#pragma once
#include <cmath>
#include <array>
#include <algorithm>
#include <cassert>

/**
 * Lightweight polyphonic triangle-wave oscillator for reference pitch audition.
 *
 * Design:
 *   - 16 pre-allocated voices (no audio-thread allocation)
 *   - Naive (non-band-limited) triangle — fine for a reference oscillator
 *   - Linear attack (5 ms) and release (100 ms) envelope to avoid clicks
 *   - Frequency can be updated per-buffer for live slider drag feedback
 *   - All internal arithmetic in float (halves memory bandwidth vs double,
 *     enables 4-wide SIMD, eliminates per-sample double→float casts)
 *   - Segmented block rendering: pre-computes envelope transition points so
 *     attack/sustain/release are tight loops without per-sample branching
 */

struct OscVoice
{
    bool  active      = false;
    bool  releasing   = false;
    int   midiNote    = -1;
    float phase       = 0.0f;
    float phaseInc    = 0.0f;
    float envelope    = 0.0f;
    float attackStep  = 0.0f;   // per-sample increment during attack
    float releaseStep = 0.0f;   // per-sample decrement during release

    void noteOn (int note, double freqHz, double sr)
    {
        midiNote    = note;
        active      = true;
        releasing   = false;
        phase       = 0.0f;
        envelope    = 0.0f;
        phaseInc    = static_cast<float> (freqHz / sr);
        const float srf = static_cast<float> (sr);
        attackStep  = 1.0f / (0.005f * srf);   // 5 ms attack
        releaseStep = 1.0f / (0.100f * srf);   // 100 ms release
    }

    void noteOff()
    {
        if (active)
            releasing = true;
    }

    void updateFrequency (double freqHz, double sr)
    {
        phaseInc = static_cast<float> (freqHz / sr);
    }

    /** Render this voice into dest (non-additive write).
     *  Returns number of samples rendered (may be < numSamples if voice
     *  deactivates during release). */
    int renderVoice (float* dest, int numSamples)
    {
        if (! active) return 0;

        if (! releasing)
        {
            if (envelope >= 1.0f)
            {
                // ── Sustain: envelope is 1.0, no multiply needed ──
                for (int s = 0; s < numSamples; ++s)
                {
                    dest[s] = 4.0f * std::abs (phase - 0.5f) - 1.0f;
                    phase += phaseInc;
                    if (phase >= 1.0f) phase -= 1.0f;
                }
                return numSamples;
            }

            // ── Attack: ramp envelope from current value to 1.0 ──
            const int attackSamples = std::min (numSamples,
                static_cast<int> (std::ceil ((1.0f - envelope) / attackStep)));

            for (int s = 0; s < attackSamples; ++s)
            {
                dest[s] = (4.0f * std::abs (phase - 0.5f) - 1.0f) * envelope;
                phase += phaseInc;
                if (phase >= 1.0f) phase -= 1.0f;
                envelope += attackStep;
            }
            envelope = std::min (envelope, 1.0f);

            // Remaining samples at full sustain (no envelope multiply)
            for (int s = attackSamples; s < numSamples; ++s)
            {
                dest[s] = 4.0f * std::abs (phase - 0.5f) - 1.0f;
                phase += phaseInc;
                if (phase >= 1.0f) phase -= 1.0f;
            }
            return numSamples;
        }

        // ── Release: ramp envelope from current value to 0.0 ──
        const int releaseSamples = std::min (numSamples,
            static_cast<int> (std::ceil (envelope / releaseStep)));

        for (int s = 0; s < releaseSamples; ++s)
        {
            dest[s] = (4.0f * std::abs (phase - 0.5f) - 1.0f) * envelope;
            phase += phaseInc;
            if (phase >= 1.0f) phase -= 1.0f;
            envelope -= releaseStep;
        }

        if (releaseSamples < numSamples)
        {
            // Voice finished release within this block
            envelope = 0.0f;
            active   = false;
            midiNote = -1;
        }
        else
        {
            envelope = std::max (envelope, 0.0f);
        }

        return releaseSamples;
    }
};

struct TriangleOscillator
{
    static constexpr int kMaxVoices    = 16;
    static constexpr int kMaxBlockSize = 8192;

    void prepare (double newSampleRate)
    {
        sampleRate = static_cast<float> (newSampleRate);
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

    /** Block render: voice output accumulated into output[] with gain.
     *  Each voice renders into a scratch buffer, then gain is applied in
     *  a separate loop (trivially auto-vectorizable by the compiler).
     *  Caller must clear output buffer first. */
    void renderBlock (float* output, int numSamples, float gain)
    {
        assert (numSamples <= kMaxBlockSize);

        for (auto& v : voices)
        {
            if (! v.active) continue;
            const int rendered = v.renderVoice (scratch, numSamples);
            for (int s = 0; s < rendered; ++s)
                output[s] += scratch[s] * gain;
        }
    }

    /** Double-precision output variant (renders internally in float). */
    void renderBlock (double* output, int numSamples, double gain)
    {
        assert (numSamples <= kMaxBlockSize);
        const float fGain = static_cast<float> (gain);

        for (auto& v : voices)
        {
            if (! v.active) continue;
            const int rendered = v.renderVoice (scratch, numSamples);
            for (int s = 0; s < rendered; ++s)
                output[s] += static_cast<double> (scratch[s] * fGain);
        }
    }

    bool hasActiveVoices() const
    {
        for (const auto& v : voices)
            if (v.active)
                return true;
        return false;
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.active)
                v.noteOff();
    }

    std::array<OscVoice, kMaxVoices> voices {};
    float sampleRate    = 44100.0f;
    int   nextStealIdx  = 0;

private:
    alignas (16) float scratch[kMaxBlockSize] {};
};
