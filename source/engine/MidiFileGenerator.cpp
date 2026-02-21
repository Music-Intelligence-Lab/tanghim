#include "MidiFileGenerator.h"
#include <algorithm>

// ── Variable-length quantity (VLQ) encoding ───────────────────────────────────

void MidiFileGenerator::writeVLQ (std::vector<uint8_t>& buffer, uint32_t value)
{
    // VLQ: 7 bits per byte, MSB set on continuation bytes
    if (value == 0)
    {
        buffer.push_back (0);
        return;
    }

    uint8_t bytes[5];
    int count = 0;
    while (value > 0)
    {
        bytes[count++] = value & 0x7F;
        value >>= 7;
    }
    // Write in reverse order (big-endian), with MSB set on all but last
    for (int i = count - 1; i >= 0; --i)
        buffer.push_back (bytes[i] | (i > 0 ? 0x80 : 0x00));
}

void MidiFileGenerator::write16BE (std::vector<uint8_t>& buffer, uint16_t value)
{
    buffer.push_back (static_cast<uint8_t> (value >> 8));
    buffer.push_back (static_cast<uint8_t> (value & 0xFF));
}

void MidiFileGenerator::write32BE (std::vector<uint8_t>& buffer, uint32_t value)
{
    buffer.push_back (static_cast<uint8_t> ((value >> 24) & 0xFF));
    buffer.push_back (static_cast<uint8_t> ((value >> 16) & 0xFF));
    buffer.push_back (static_cast<uint8_t> ((value >> 8) & 0xFF));
    buffer.push_back (static_cast<uint8_t> (value & 0xFF));
}

// ── MIDI file generation ──────────────────────────────────────────────────────

std::vector<uint8_t> MidiFileGenerator::generate (const MaqamInfo& info)
{
    std::vector<uint8_t> file;

    // ── Build track name ──────────────────────────────────────────────────────
    // Format: maqām_rāst_al-rāst_C3_Do3
    juce::String trackName = info.maqamDisplay.replace (" ", "_")
                           + "_al-" + info.tonicPaoDisplay
                           + "_" + info.tonicIpn
                           + "_" + info.tonicSolfege.replace (" ", "");
    auto trackNameUtf8 = trackName.toRawUTF8();
    size_t trackNameLen = std::strlen (trackNameUtf8);

    // ── Build track data ──────────────────────────────────────────────────────
    std::vector<uint8_t> track;

    // Track name meta event: FF 03 len <name>
    writeVLQ (track, 0);  // delta time
    track.push_back (0xFF);
    track.push_back (0x03);
    writeVLQ (track, static_cast<uint32_t> (trackNameLen));
    for (size_t i = 0; i < trackNameLen; ++i)
        track.push_back (static_cast<uint8_t> (trackNameUtf8[i]));

    // Time signature meta event: FF 58 04 nn dd cc bb
    // 4/4 time, 24 MIDI clocks per metronome click, 8 32nd notes per beat
    writeVLQ (track, 0);
    track.push_back (0xFF);
    track.push_back (0x58);
    track.push_back (0x04);
    track.push_back (0x04);  // numerator: 4
    track.push_back (0x02);  // denominator: 2^2 = 4
    track.push_back (0x24);  // 36 MIDI clocks per metronome click
    track.push_back (0x08);  // 8 32nd notes per beat

    // Sort notes ascending for consistent output
    std::vector<int> sortedNotes = info.midiNotes;
    std::sort (sortedNotes.begin(), sortedNotes.end());

    // Note On events (all at delta 0, channel 0, velocity 100)
    for (int note : sortedNotes)
    {
        if (note < 0 || note > 127) continue;
        writeVLQ (track, 0);  // delta time
        track.push_back (0x90);  // Note On, channel 0
        track.push_back (static_cast<uint8_t> (note));
        track.push_back (0x64);  // velocity 100
    }

    // Note Off events (after 96 ticks = 1 quarter note, channel 0)
    bool first = true;
    for (int note : sortedNotes)
    {
        if (note < 0 || note > 127) continue;
        writeVLQ (track, first ? 96 : 0);  // first note has delta, rest are 0
        first = false;
        track.push_back (0x80);  // Note Off, channel 0
        track.push_back (static_cast<uint8_t> (note));
        track.push_back (0x40);  // release velocity 64
    }

    // End of track meta event: FF 2F 00
    writeVLQ (track, 0);
    track.push_back (0xFF);
    track.push_back (0x2F);
    track.push_back (0x00);

    // ── Build file header ─────────────────────────────────────────────────────
    // MThd chunk
    file.push_back ('M'); file.push_back ('T');
    file.push_back ('h'); file.push_back ('d');
    write32BE (file, 6);     // header length
    write16BE (file, 0);     // format 0 (single track)
    write16BE (file, 1);     // 1 track
    write16BE (file, 96);    // 96 ticks per quarter note

    // MTrk chunk
    file.push_back ('M'); file.push_back ('T');
    file.push_back ('r'); file.push_back ('k');
    write32BE (file, static_cast<uint32_t> (track.size()));
    file.insert (file.end(), track.begin(), track.end());

    return file;
}

juce::String MidiFileGenerator::buildFilename (const MaqamInfo& info)
{
    // Format: maqām_rāst_al-rāst_C3_Do3.mid
    return info.maqamDisplay.replace (" ", "_")
         + "_al-" + info.tonicPaoDisplay
         + "_" + info.tonicIpn
         + "_" + info.tonicSolfege.replace (" ", "")
         + ".mid";
}
