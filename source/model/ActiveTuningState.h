#pragma once
#include "PitchClass.h"
#include <array>
#include <vector>
#include <cmath>

/**
 * All available variants for one chromatic slider position (C…B).
 *
 * "Variants" are all pitch classes in the current tuning system whose IPN
 * reference resolves to the same chromatic note. For example, in Ibn Sīnā's
 * 17-tone system, chromatic position E (index 4) has two variants:
 *   variant 0: segāh   (E-b3, ~341 Hz)
 *   variant 1: būselīk (E3,   ~330 Hz)
 * The slider snaps between these positions. If there is only one variant,
 * the slider is locked.
 */
struct ChromaticNoteVariants
{
    juce::String ipnReference;          // "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
    std::vector<PitchClass> variants;   // Ordered by pitch (ascending cents)
    int selectedIndex = 0;
    double centsOffset = 0.0;           // Arbitrary cents deviation from 12-EDO; source of truth for tuning output

    bool isLocked()     const { return variants.size() <= 1; }
    int  variantCount() const { return (int) variants.size(); }

    const PitchClass* selectedVariant() const
    {
        if (variants.empty()) return nullptr;
        return &variants[(size_t) juce::jlimit (0, variantCount() - 1, selectedIndex)];
    }
};

/**
 * The complete active tuning state: 12 chromatic slots, each holding the
 * available variants and the currently selected one, plus per-MIDI-note
 * override indices for octave-independent control.
 *
 * This is the single source of truth for all tuning output (MTS-ESP table,
 * MPE pitch bend values, mono pitch bend values).
 */
struct ActiveTuningState
{
    std::array<ChromaticNoteVariants, 12> slots;

    /// Per-MIDI-note variant index overrides.
    /// -1 = use the chromatic slot's selectedIndex (default / no override).
    /// >= 0 = use this variant index for this specific MIDI note.
    std::array<int, 128> perNoteVariantOverrides;

    /// Per-MIDI-note cents deviation overrides.
    /// NaN = no override (use chromatic slot's centsOffset).
    /// Any other value = use this cents deviation for this specific MIDI note.
    std::array<double, 128> perNoteCentsOverrides;

    ActiveTuningState()
    {
        perNoteVariantOverrides.fill (-1);
        clearPerNoteCentsOverrides();
    }

    // ── Per-note resolution ────────────────────────────────────────────────────

    /** Return the effective variant index for a given MIDI note,
     *  accounting for per-note overrides. */
    int effectiveVariantIndex (int midi) const
    {
        const int chromaticIdx = midi % 12;
        const int override = (midi >= 0 && midi < 128)
                                 ? perNoteVariantOverrides[(size_t) midi]
                                 : -1;
        const int idx = (override >= 0) ? override : slots[(size_t) chromaticIdx].selectedIndex;
        return juce::jlimit (0, std::max (0, slots[(size_t) chromaticIdx].variantCount() - 1), idx);
    }

    /** Return the effective selected variant for a given MIDI note. */
    const PitchClass* effectiveVariant (int midi) const
    {
        const int chromaticIdx = midi % 12;
        const auto& slot = slots[(size_t) chromaticIdx];
        if (slot.variants.empty()) return nullptr;
        return &slot.variants[(size_t) effectiveVariantIndex (midi)];
    }

    /** Clear all per-note variant overrides (reset to chromatic-wide behaviour). */
    void clearPerNoteOverrides()
    {
        perNoteVariantOverrides.fill (-1);
    }

    /** Clear all per-note cents overrides (reset to chromatic slot centsOffset). */
    void clearPerNoteCentsOverrides()
    {
        for (auto& v : perNoteCentsOverrides)
            v = std::numeric_limits<double>::quiet_NaN();
    }

    /** Check if a per-note cents override is active for a given MIDI note. */
    bool hasPerNoteCentsOverride (int midi) const
    {
        if (midi < 0 || midi >= 128) return false;
        return ! std::isnan (perNoteCentsOverrides[(size_t) midi]);
    }

    /** Get effective cents for a MIDI note: per-note override if set, else chromatic slot. */
    double effectiveCents (int midi) const
    {
        if (midi >= 0 && midi < 128 && ! std::isnan (perNoteCentsOverrides[(size_t) midi]))
            return perNoteCentsOverrides[(size_t) midi];
        return slots[(size_t) (midi % 12)].centsOffset;
    }

    // ── Frequency / cents tables ──────────────────────────────────────────────

    /**
     * Build a 128-entry frequency table (Hz) for MTS-ESP.
     * Each MIDI note uses the chromatic slot's centsOffset (the source of truth
     * for tuning output — set by free slider drag or synced from variant selection).
     */
    std::array<double, 128> buildFrequencyTable() const
    {
        std::array<double, 128> table;
        for (int midi = 0; midi < 128; ++midi)
        {
            const double cents = effectiveCents (midi);

            double freq;
            if (cents != 0.0)
            {
                const double centsFromA440 = (midi - 69) * 100.0 + cents;
                freq = 440.0 * std::pow (2.0, centsFromA440 / 1200.0);
            }
            else
            {
                freq = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
            }
            table[(size_t) midi] = freq;
        }
        return table;
    }

    /**
     * Build a 128-entry cents-deviation table.
     * Each entry is the deviation in cents from standard 12-EDO for that MIDI note.
     * Per-note cents overrides take priority over chromatic slot centsOffset.
     */
    std::array<double, 128> buildCentsDeviationTable() const
    {
        std::array<double, 128> table;
        table.fill (0.0);
        for (int midi = 0; midi < 128; ++midi)
            table[(size_t) midi] = effectiveCents (midi);
        return table;
    }

    bool isEmpty() const
    {
        for (const auto& s : slots)
            if (! s.variants.empty()) return false;
        return true;
    }
};
