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

    /** Scan the registry directory and count non-stale receiver files. */
    static ReceiverCounts scan (double staleCutoffSeconds = 5.0)
    {
        ReceiverCounts counts;
        auto dir = getRegistryDir();
        if (! dir.isDirectory()) return counts;

        auto now    = juce::Time::getCurrentTime();
        auto cutoff = juce::RelativeTime (staleCutoffSeconds);

        for (const auto& entry : juce::RangedDirectoryIterator (dir, false))
        {
            auto age = now - entry.getFile().getLastModificationTime();
            if (age > cutoff) continue;

            if (entry.getFile().hasFileExtension ("mpe"))
                counts.mpeReceivers++;
            else if (entry.getFile().hasFileExtension ("monopb"))
                counts.monoPbReceivers++;
        }
        return counts;
    }

    /** Clean up stale files (crashed receivers that left orphaned files). */
    static void cleanStale (double staleCutoffSeconds = 10.0)
    {
        auto dir = getRegistryDir();
        if (! dir.isDirectory()) return;

        auto now    = juce::Time::getCurrentTime();
        auto cutoff = juce::RelativeTime (staleCutoffSeconds);

        for (const auto& entry : juce::RangedDirectoryIterator (dir, false))
        {
            if ((now - entry.getFile().getLastModificationTime()) > cutoff)
                entry.getFile().deleteFile();
        }
    }

private:
    static const char* extension (bool isMpe)
    {
        return isMpe ? ".mpe" : ".monopb";
    }
};
