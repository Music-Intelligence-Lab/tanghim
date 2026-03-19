#pragma once
#include "../model/PitchClass.h"
#include "../model/TuningSystem.h"
#include "../model/MaqamListEntry.h"
#include <juce_core/juce_core.h>
#include <map>
#include <set>
#include <vector>

/**
 * Disk-backed in-memory cache for DiArMaqAr API data.
 *
 * Cache is keyed by "tuningSystemId:startingNote".
 * Each entry stores the version timestamp from the API so we can detect updates
 * without re-downloading everything.
 *
 * Lazy loading: On startup, only file names are scanned. Actual JSON
 * deserialization happens on first access via getData(). Individual entries
 * are saved to disk immediately when modified (not just on exit).
 *
 * Thread safety: All public methods are guarded by an internal CriticalSection
 * and safe to call from any thread.
 */
class ApiDataCache
{
public:
    struct TuningData
    {
        std::vector<PitchClass>          pitchClasses;       // All pitches (pitchClassDataType=all)
        std::vector<MaqamListEntry>      maqamList;          // Maqamat with degrees + transpositions
        juce::String                     tuningSystemVersion; // ISO 8601 from API
        juce::String                     lastChecked;         // ISO 8601 when we last polled
    };

    ApiDataCache();
    ~ApiDataCache();

    // ── Tuning systems list ───────────────────────────────────────────────────
    void setTuningSystemsList (std::vector<TuningSystem> systems);
    const std::vector<TuningSystem>& getTuningSystemsList() const;
    bool hasTuningSystemsList() const;

    // ── Per-system tuning data ────────────────────────────────────────────────
    juce::String makeKey (const juce::String& systemId,
                          const juce::String& startingNote) const;

    bool          hasData   (const juce::String& systemId, const juce::String& startingNote) const;
    /** Whether the cache entry has a non-empty maqam list. */
    bool          hasMaqamList (const juce::String& systemId, const juce::String& startingNote) const;
    const TuningData& getData (const juce::String& systemId, const juce::String& startingNote) const;
    void          storeData (const juce::String& systemId, const juce::String& startingNote, TuningData data);

    /** Update only the lastChecked timestamp for an existing entry. */
    void updateLastChecked (const juce::String& systemId, const juce::String& startingNote);

    /** Store fetched maqam list into an existing cache entry. */
    void updateMaqamList (const juce::String& systemId, const juce::String& startingNote,
                          const std::vector<MaqamListEntry>& maqamList);

    /** Whether data is already in memory (not just a lazy key on disk). */
    bool isInMemory (const juce::String& systemId, const juce::String& startingNote) const;

    /** Preload a lazy cache entry from disk on the current thread.
     *  Safe to call from a background thread — reads + parses JSON outside the lock,
     *  then briefly locks to insert into cache. */
    void preload (const juce::String& systemId, const juce::String& startingNote);

    // ── Disk persistence ──────────────────────────────────────────────────────
    juce::File getCacheDirectory() const;
    void       loadFromDisk();
    void       clearAll();

private:
    mutable juce::CriticalSection lock;
    std::vector<TuningSystem>     tuningSystemsList;
    bool                          hasSystems = false;

    // Loaded entries (deserialized from disk or from API)
    mutable std::map<juce::String, TuningData> cache;

    // Keys found on disk but not yet deserialized (lazy loading)
    mutable std::set<juce::String> lazyKeys;

    juce::File getCacheFile (const juce::String& key) const;
    juce::File getSystemsListFile() const;
    bool       ensureCacheDirectory() const;

    // Lazy-load a single entry from disk into cache (must be called under lock)
    void ensureLoaded (const juce::String& key) const;

    // Save a single entry to disk (must be called under lock)
    void saveEntryToDisk (const juce::String& key, const TuningData& data) const;
    void saveSystemsListToDisk() const;

    static juce::var tuningDataToJson (const TuningData& data);
    static TuningData jsonToTuningData (const juce::var& json);
    static juce::var pitchClassToJson (const PitchClass& pc);
    static PitchClass jsonToPitchClass (const juce::var& obj);

};
