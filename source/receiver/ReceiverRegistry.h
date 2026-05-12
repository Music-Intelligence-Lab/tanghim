#pragma once
#include <juce_core/juce_core.h>

/**
 * File-based receiver registry for IPC between Transmitter and Receiver plugins.
 *
 * Since the Transmitter and Receiver are separate VST3 bundles (different shared libraries),
 * they cannot share in-process state. MTS-ESP only provides MTS_GetNumClients() (a single
 * integer count) with no client enumeration or metadata.
 *
 * This registry allows the Transmitter to discover how many of our Receivers are loaded
 * and what mode each is in (MPE vs Mono PB), so it can display 3 separate badges:
 *   MTS-ESP (native synths) | MPE (our receivers) | Mono PB (our receivers)
 *
 * Each Receiver writes a file to ~/Library/Tanghim/receivers/{uuid}.{mpe|monopb}.
 * The Transmitter scans this directory to count receivers by mode.
 * Heartbeat (mtime touch) + stale detection handles crash recovery.
 */

struct ReceiverCounts
{
    int mpeReceivers  = 0;
    int monoPbReceivers = 0;

    bool operator== (const ReceiverCounts& o) const
    {
        return mpeReceivers == o.mpeReceivers && monoPbReceivers == o.monoPbReceivers;
    }
    bool operator!= (const ReceiverCounts& o) const { return ! (*this == o); }
};

class ReceiverRegistry
{
public:
    // ── Registry directory ────────────────────────────────────────────────

    static juce::File getRegistryDir()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Tanghim")
                   .getChildFile ("receivers");
    }

    // ── Announcer API (used by Receiver) ──────────────────────────────────

    /** Create a registry file for this receiver instance. Returns the UUID. */
    static juce::String announce (bool isMpe)
    {
        auto dir = getRegistryDir();
        dir.createDirectory();

        auto uuid = juce::Uuid().toString();
        auto file = dir.getChildFile (uuid + extension (isMpe));
        file.create();
        return uuid;
    }

    /** Update the heartbeat (touch the file to update mtime). */
    static void heartbeat (const juce::String& uuid, bool isMpe)
    {
        auto file = getRegistryDir().getChildFile (uuid + extension (isMpe));
        if (file.exists())
            file.setLastModificationTime (juce::Time::getCurrentTime());
    }

    /** Switch mode: delete old file, create new one with new extension. */
    static void switchMode (const juce::String& uuid, bool oldIsMpe, bool newIsMpe)
    {
        if (oldIsMpe == newIsMpe) return;
        auto dir = getRegistryDir();
        dir.getChildFile (uuid + extension (oldIsMpe)).deleteFile();
        dir.getChildFile (uuid + extension (newIsMpe)).create();
    }

    /** Remove the registry file (called on destruction). */
    static void deannounce (const juce::String& uuid, bool isMpe)
    {
        getRegistryDir().getChildFile (uuid + extension (isMpe)).deleteFile();
    }

    // ── Scanner API (used by Transmitter) ─────────────────────────────────

    /** Scan the registry directory and count active receiver files.
     *  VST3 receivers heartbeat at 1 Hz; M4L receivers heartbeat every 30 s.
     *  Files whose first line is "closed" are skipped immediately (written by
     *  M4L freebang for instant badge update on device deletion).
     *  Stale cutoff is 60 s — safe margin above the 30 s M4L heartbeat. */
    static ReceiverCounts scan (double staleCutoffSeconds = 60.0)
    {
        ReceiverCounts counts;
        auto dir = getRegistryDir();
        if (! dir.isDirectory()) return counts;

        auto now    = juce::Time::getCurrentTime();
        auto cutoff = juce::RelativeTime (staleCutoffSeconds);

        for (const auto& entry : juce::RangedDirectoryIterator (dir, false))
        {
            auto file = entry.getFile();
            auto age  = now - file.getLastModificationTime();
            if (age > cutoff) continue;

            // M4L devices write "closed" on freebang — skip immediately
            if (file.loadFileAsString().trim().startsWith ("closed")) continue;

            if (file.hasFileExtension ("mpe"))
                counts.mpeReceivers++;
            else if (file.hasFileExtension ("monopb"))
                counts.monoPbReceivers++;
        }
        return counts;
    }

    /** Clean up stale and closed files (crashed receivers, freed M4L devices). */
    static void cleanStale (double staleCutoffSeconds = 90.0)
    {
        auto dir = getRegistryDir();
        if (! dir.isDirectory()) return;

        auto now    = juce::Time::getCurrentTime();
        auto cutoff = juce::RelativeTime (staleCutoffSeconds);

        for (const auto& entry : juce::RangedDirectoryIterator (dir, false))
        {
            auto file = entry.getFile();
            if ((now - file.getLastModificationTime()) > cutoff)
                file.deleteFile();
        }
    }

private:
    static const char* extension (bool isMpe)
    {
        return isMpe ? ".mpe" : ".monopb";
    }
};
