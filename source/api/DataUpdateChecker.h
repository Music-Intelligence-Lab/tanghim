#pragma once
#include "DiArMaqArClient.h"
#include "ApiDataCache.h"
#include <juce_core/juce_core.h>
#include <functional>
#include <vector>

/**
 * Checks the DiArMaqAr API for data updates by comparing ISO 8601 version
 * timestamps on API responses against cached versions.
 *
 * Strategy:
 *   1. Fetch GET /tuning-systems (lightweight — returns metadata only)
 *   2. Compare each system's "version" field against the stored version in cache
 *   3. For any system whose version is newer, mark it as stale
 *   4. Re-fetch pitch classes + 12-pitch-class-sets for stale systems in background
 *   5. Report results via callbacks on the message thread
 */
class DataUpdateChecker
{
public:
    DataUpdateChecker (DiArMaqArClient& client, ApiDataCache& cache);

    /**
     * Async check for updates. Calls onUpdatesFound with a list of system IDs
     * that were updated (and re-fetched), or onNoUpdates if everything is current.
     * onError is called if the network check itself fails.
     */
    void checkForUpdates (
        std::function<void (std::vector<juce::String> updatedSystemIds)> onUpdatesFound,
        std::function<void()>                                             onNoUpdates,
        std::function<void (juce::String)>                                onError = {});

    /** Force-refresh data for a specific tuning system + starting note. */
    void forceRefresh (const juce::String& systemId,
                       const juce::String& startingNote,
                       std::function<void()> onComplete = {},
                       std::function<void (juce::String)> onError = {});

    bool isChecking() const { return checking.load(); }

private:
    DiArMaqArClient& client;
    ApiDataCache&    cache;
    std::atomic<bool> checking { false };

    static bool isVersionNewer (const juce::String& apiVersion,
                                const juce::String& cachedVersion);
};
