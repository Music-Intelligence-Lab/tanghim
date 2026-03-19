#include "MidiFileGenerator.h"
#include <algorithm>

// ── ASCII transliteration for MIDI text meta events ──────────────────────────
// MIDI spec uses ASCII for text events. DAWs like Ableton interpret raw bytes
// as Mac Roman, garbling UTF-8 diacritics (e.g. ā → ƒÅ). This maps Arabic
// transliteration diacritics to their ASCII base letters.

static juce::String toAscii (const juce::String& input)
{
    juce::String result;
    for (auto cp = input.getCharPointer(); ! cp.isEmpty();)
    {
        auto c = cp.getAndAdvance();
        switch (c)
        {
            // Lowercase: macron vowels + dotted consonants
            case 0x0101: result += 'a'; break; // ā
            case 0x012B: result += 'i'; break; // ī
            case 0x016B: result += 'u'; break; // ū
            case 0x1E25: result += 'h'; break; // ḥ
            case 0x1E63: result += 's'; break; // ṣ
            case 0x1E0D: result += 'd'; break; // ḍ
            case 0x1E6D: result += 't'; break; // ṭ
            case 0x1E93: result += 'z'; break; // ẓ
            case 0x0121: result += 'g'; break; // ġ
            // Uppercase equivalents
            case 0x0100: result += 'A'; break; // Ā
            case 0x012A: result += 'I'; break; // Ī
            case 0x016A: result += 'U'; break; // Ū
            case 0x1E24: result += 'H'; break; // Ḥ
            case 0x1E62: result += 'S'; break; // Ṣ
            case 0x1E0C: result += 'D'; break; // Ḍ
            case 0x1E6C: result += 'T'; break; // Ṭ
            case 0x1E92: result += 'Z'; break; // Ẓ
            case 0x0120: result += 'G'; break; // Ġ
            default:
                if (c >= 32 && c < 128)
                    result += static_cast<char> (c);
                else
                    result += '-';  // Replace unknown non-ASCII with dash
                break;
        }
    }
    return result;
}

// Replace filesystem-unsafe characters (/ : \) with dash.
// Keeps UTF-8 diacritics intact.
static juce::String sanitizeForFilename (const juce::String& input)
{
    juce::String result;
    for (auto cp = input.getCharPointer(); ! cp.isEmpty();)
    {
        auto c = cp.getAndAdvance();
        if (c == '/' || c == ':' || c == '\\')
            result += '-';
        else if (c >= 32)
            result += juce::String::charToString (c);
    }
    return result;
}

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

// ── Name building helpers ────────────────────────────────────────────────────
// Format: maqamname_(PAOname-IPN-solfege)
// e.g. "maqam rast_(rast-C3-Do3)" or "maqam_rast_(rast-C3-Do3).mid"

static juce::String buildNameBase (const juce::String& maqamDisplay,
                                   const juce::String& tonicPao,
                                   const juce::String& tonicIpn,
                                   const juce::String& tonicSolfege)
{
    return maqamDisplay.replace (" ", "_")
         + "_(" + tonicPao
         + "-" + tonicIpn
         + "-" + tonicSolfege.replace (" ", "")
         + ")";
}

// ── MIDI file generation ──────────────────────────────────────────────────────

std::vector<uint8_t> MidiFileGenerator::generate (const MaqamInfo& info)
{
    std::vector<uint8_t> file;

    // ── Build track name (ASCII) ─────────────────────────────────────────────
    // MIDI text meta events use ASCII. Ableton interprets raw bytes as Mac Roman,
    // so UTF-8 diacritics get garbled. Use ASCII transliteration for the track name;
    // the UTF-8 filename carries the diacritics for filesystem display.
    juce::String trackName = toAscii (buildNameBase (info.maqamDisplay,
                                                     info.tonicPaoDisplay,
                                                     info.tonicIpn,
                                                     info.tonicSolfege));
    auto trackNameBytes = trackName.toRawUTF8();  // pure ASCII at this point
    size_t trackNameLen = std::strlen (trackNameBytes);

    // ── Build track data ─────────────────────────────────────────────────────
    std::vector<uint8_t> track;

    // Track name meta event: FF 03 len <name>
    writeVLQ (track, 0);  // delta time
    track.push_back (0xFF);
    track.push_back (0x03);
    writeVLQ (track, static_cast<uint32_t> (trackNameLen));
    for (size_t i = 0; i < trackNameLen; ++i)
        track.push_back (static_cast<uint8_t> (trackNameBytes[i]));

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

    // Program change (preset selection) — before notes so preset activates first
    if (info.presetIndex >= 0 && info.presetIndex < 8)
    {
        writeVLQ (track, 0);  // delta time
        track.push_back (0xC0);  // Program Change, channel 0
        track.push_back (static_cast<uint8_t> (info.presetIndex));
    }

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
    // Format: maqām_rāst_(rāst-C3-Do3).mid (UTF-8 for filesystem)
    return sanitizeForFilename (buildNameBase (info.maqamDisplay,
                                              info.tonicPaoDisplay,
                                              info.tonicIpn,
                                              info.tonicSolfege))
         + ".mid";
}
