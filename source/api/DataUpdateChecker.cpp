#include "DataUpdateChecker.h"

DataUpdateChecker::DataUpdateChecker (DiArMaqArClient& c, ApiDataCache& ch,
                                      std::weak_ptr<std::atomic<bool>> a)
    : client (c), cache (ch), alive (std::move (a)) {}

// Local copy of the isAlive helper (also defined in PluginProcessor.h)
static bool isAlive (const std::weak_ptr<std::atomic<bool>>& w)
{
    auto f = w.lock();
    return f && f->load (std::memory_order_acquire);
}

void DataUpdateChecker::checkForUpdates (
    std::function<void (std::vector<juce::String>)> onUpdatesFound,
    std::function<void()>                            onNoUpdates,
    std::function<void (juce::String)>               onError)
{
    if (checking.exchange (true)) return; // already running

    auto weak = alive;
    client.fetchTuningSystems (
        [this, weak, onUpdatesFound, onNoUpdates] (std::vector<TuningSystem> systems)
        {
            if (! isAlive (weak)) { checking.store (false); return; }
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
            // Each stale system requires both pitch classes AND maqam list — remaining
            // is only decremented after both (or after maqam fetch fails).
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
                    [this, weak, systemId, startingNote, systemVersion, remaining, updated, onUpdatesFound]
                    (std::vector<PitchClass> pcs)
                    {
                        if (! isAlive (weak)) { checking.store (false); return; }
                        ApiDataCache::TuningData data;
                        data.pitchClasses        = std::move (pcs);
                        data.tuningSystemVersion = systemVersion;
                        data.lastChecked         = juce::Time::getCurrentTime().toISO8601 (true);
                        cache.storeData (systemId, startingNote, std::move (data));

                        // Chain maqam list fetch — decrement remaining after it completes
                        client.fetchMaqamList (systemId, startingNote,
                            [this, weak, systemId, startingNote, remaining, updated, onUpdatesFound]
                            (std::vector<MaqamListEntry> maqamat)
                            {
                                if (! isAlive (weak)) { checking.store (false); return; }
                                cache.updateMaqamList (systemId, startingNote, maqamat);

                                if (--(*remaining) == 0)
                                {
                                    checking.store (false);
                                    if (onUpdatesFound) onUpdatesFound (*updated);
                                }
                            },
                            [this, weak, remaining, updated, onUpdatesFound]
                            (juce::String)
                            {
                                // Maqam fetch failed — still count as done
                                if (! isAlive (weak)) { checking.store (false); return; }
                                if (--(*remaining) == 0)
                                {
                                    checking.store (false);
                                    if (onUpdatesFound) onUpdatesFound (*updated);
                                }
                            });
                    });
            }
        },
        [this, weak, onError] (juce::String err)
        {
            checking.store (false);
            if (! isAlive (weak)) return;
            if (onError) onError (err);
        });
}

bool DataUpdateChecker::checkOnly (
    std::function<void (std::vector<juce::String>)> onStaleFound,
    std::function<void()>                            onAllCurrent,
    std::function<void (juce::String)>               onError)
{
    if (checking.exchange (true)) return false; // already running

    auto weak = alive;
    client.fetchTuningSystems (
        [this, weak, onStaleFound, onAllCurrent] (std::vector<TuningSystem> systems)
        {
            if (! isAlive (weak)) { checking.store (false); return; }
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

            checking.store (false);

            if (staleSystemIds.empty())
            {
                if (onAllCurrent) onAllCurrent();
            }
            else
            {
                if (onStaleFound) onStaleFound (staleSystemIds);
            }
        },
        [this, weak, onError] (juce::String err)
        {
            checking.store (false);
            if (! isAlive (weak)) return;
            if (onError) onError (err);
        });
    return true;
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

    auto weak = alive;
    client.fetchPitchClasses (systemId, startingNote,
        [this, weak, systemId, startingNote, systemVersion, onComplete] (std::vector<PitchClass> pcs)
        {
            if (! isAlive (weak)) return;
            ApiDataCache::TuningData data;
            data.pitchClasses        = std::move (pcs);
            data.tuningSystemVersion = systemVersion;
            data.lastChecked         = juce::Time::getCurrentTime().toISO8601 (true);
            cache.storeData (systemId, startingNote, std::move (data));
            if (onComplete) onComplete();
        },
        std::move (onError));
}

bool DataUpdateChecker::isVersionNewer (const juce::String& apiVersion,
                                         const juce::String& cachedVersion)
{
    if (cachedVersion.isEmpty()) return true;  // nothing cached → treat as new
    if (apiVersion.isEmpty())    return false; // no API version → assume same

    // ISO 8601 timestamps sort lexicographically — simple string comparison works
    return apiVersion > cachedVersion;
}
