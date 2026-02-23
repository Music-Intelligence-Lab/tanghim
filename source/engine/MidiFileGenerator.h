#pragma once
#include <juce_core/juce_core.h>
#include <vector>

/**
 * Generates a Standard MIDI File (SMF) Type 0 containing the notes of a maqam scale.
 *
 * Filename format: maqām_rāst_(rāst-C3-Do3).mid (UTF-8)
 * Track name: ASCII transliteration (MIDI spec = ASCII; Ableton reads bytes as Mac Roman)
 * - All scale degree notes are played simultaneously as a chord
 * - Single track, 96 ticks per quarter note
 * - Notes held for 1 quarter note then released
 */
class MidiFileGenerator
{
public:
    struct MaqamInfo
    {
        juce::String maqamDisplay;      // e.g. "maqām rāst"
        juce::String tonicPaoDisplay;   // e.g. "rāst"
        juce::String tonicIpn;          // e.g. "C3"
        juce::String tonicSolfege;      // e.g. "Do 3"
        std::vector<int> midiNotes;     // MIDI note numbers for scale degrees
    };

    /**
     * Generate MIDI file data for a maqam.
     * @param info Maqam metadata and note list
     * @return Raw MIDI file bytes (SMF Type 0)
     */
    static std::vector<uint8_t> generate (const MaqamInfo& info);

    /**
     * Build the filename for a maqam MIDI file.
     * Format: maqām_rāst_al-rāst_C3_Do3.mid
     * @param info Maqam metadata
     * @return Filename with .mid extension
     */
    static juce::String buildFilename (const MaqamInfo& info);

private:
    /** Write a variable-length quantity (VLQ) to a buffer. */
    static void writeVLQ (std::vector<uint8_t>& buffer, uint32_t value);

    /** Write a 16-bit big-endian value. */
    static void write16BE (std::vector<uint8_t>& buffer, uint16_t value);

    /** Write a 32-bit big-endian value. */
    static void write32BE (std::vector<uint8_t>& buffer, uint32_t value);
};
