#include "DataUpdateChecker.h"

DataUpdateChecker::DataUpdateChecker (DiArMaqArClient& c, ApiDataCache& ch)
    : client (c), cache (ch) {}

void DataUpdateChecker::checkForUpdates (
    std::function<void (std::vector<juce::String>)> onUpdatesFound,
    std::function<void()>                            onNoUpdates,
    std::function<void (juce::String)>               onError)
{
    if (checking.exchange (true)) return; // already running

    client.fetchTuningSystems (
        [this, onUpdatesFound, onNoUpdates] (std::vector<TuningSystem> systems)
        {
            // Update the systems list in cache regardless
            cache.setTuningSystemsList (systems);

            // Find systems with cached data whose version has changed
            std::vector<juce::String> staleSystemIds;
            for (const auto& ts : systems)
            {
                if (ts.startingNoteIds.isEmpty()) continue;
                const juce::String& startingNote = ts.startingNoteIds[0];

                if (cache.hasData (ts.id, startingNote))
                {
                    const auto& cached = cache.getData (ts.id, startingNote);
                    if (isVersionNewer (ts.version, cached.tuningSystemVersion))
                        staleSystemIds.push_back (ts.id);
                    else
                        cache.updateLastChecked (ts.id, startingNote);
                }
            }

            if (staleSystemIds.empty())
            {
                checking.store (false);
                if (onNoUpdates) onNoUpdates();
                return;
            }

            // Re-fetch stale systems. Count down completions.
            auto remaining = std::make_shared<std::atomic<int>> ((int) staleSystemIds.size());
            auto updated   = std::make_shared<std::vector<juce::String>> (staleSystemIds);

            for (const auto& systemId : staleSystemIds)
            {
                // Find the system to get its default starting note and version
                juce::String startingNote;
                juce::String systemVersion;
                for (const auto& ts : systems)
                    if (ts.id == systemId && ts.startingNoteIds.size() > 0)
                        { startingNote = ts.startingNoteIds[0]; systemVersion = ts.version; break; }

                if (startingNote.isEmpty()) { --(*remaining); continue; }

                client.fetchPitchClasses (systemId, startingNote,
                    [this, systemId, startingNote, systemVersion, remaining, updated, onUpdatesFound]
                    (std::vector<PitchClass> pcs)
                    {
                        ApiDataCache::TuningData data;
                        data.pitchClasses        = std::move (pcs);
                        data.tuningSystemVersion = systemVersion;
                        data.lastChecked         = juce::Time::getCurrentTime().toISO8601 (true);
                        cache.storeData (systemId, startingNote, std::move (data));

                        if (--(*remaining) == 0)
                        {
                            checking.store (false);
                            if (onUpdatesFound) onUpdatesFound (*updated);
                        }
                    });
            }
        },
        [this, onError] (juce::String err)
        {
            checking.store (false);
            if (onError) onError (err);
        });
}

void DataUpdateChecker::forceRefresh (
    const juce::String& systemId,
    const juce::String& startingNote,
    std::function<void()>               onComplete,
    std::function<void (juce::String)>  onError)
{
    // Look up current version from the systems list
    juce::String systemVersion;
    for (const auto& ts : cache.getTuningSystemsList())
        if (ts.id == systemId) { systemVersion = ts.version; break; }

    client.fetchPitchClasses (systemId, startingNote,
        [this, systemId, startingNote, systemVersion, onComplete] (std::vector<PitchClass> pcs)
        {
            ApiDataCache::TuningData data;
            data.pitchClasses        = std::move (pcs);
            data.tuningSystemVersion = systemVersion;
            data.lastChecked         = juce::Time::getCurrentTime().toISO8601 (true);
            cache.storeData (systemId, startingNote, std::move (data));
            if (onComplete) onComplete();
        },
        std::move (onError));

    // Also re-fetch 12-pitch-class sets
    client.fetchTwelvePitchClassSets (systemId, startingNote,
        [this, systemId, startingNote] (std::vector<TwelvePitchClassSet> sets)
        {
            // Merge into existing cache entry
            if (cache.hasData (systemId, startingNote))
            {
                // Rebuild entry with updated sets
                // (TuningData doesn't store sets directly; the processor fetches them
                //  separately. This triggers a cache-miss next time sets are needed.)
            }
        });
}

bool DataUpdateChecker::isVersionNewer (const juce::String& apiVersion,
                                         const juce::String& cachedVersion)
{
    if (cachedVersion.isEmpty()) return true;  // nothing cached → treat as new
    if (apiVersion.isEmpty())    return false; // no API version → assume same

    // ISO 8601 timestamps sort lexicographically — simple string comparison works
    return apiVersion > cachedVersion;
}
