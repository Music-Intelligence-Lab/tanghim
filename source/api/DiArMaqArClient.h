#pragma once
#include "../model/PitchClass.h"
#include "../model/TuningSystem.h"
#include "../model/MaqamListEntry.h"
#include "ApiResponseParser.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>
#include <queue>

/**
 * Async HTTP client for the DiArMaqAr REST API.
 *
 * All network I/O runs on a dedicated background thread. Callbacks are always
 * dispatched to the JUCE message thread via MessageManager::callAsync(), so it
 * is safe to update the UI directly from them.
 *
 * Base URL: https://diarmaqar.netlify.app/api
 */
class DiArMaqArClient : private juce::Thread
{
public:
    static constexpr const char* BASE_URL = "https://diarmaqar.netlify.app/api";

    DiArMaqArClient();
    ~DiArMaqArClient() override;

    using ErrorCb = std::function<void (juce::String errorMessage)>;

    // ── High-level API calls ──────────────────────────────────────────────────

    /** GET /tuning-systems */
    void fetchTuningSystems (
        std::function<void (std::vector<TuningSystem>)> onSuccess,
        ErrorCb onError = {});

    /**
     * GET /tuning-systems/{id}/{startingNote}/pitch-classes?pitchClassDataType=all
     * Returns all pitch data including midiNoteDeviation, englishName, cents, frequency.
     */
    void fetchPitchClasses (
        const juce::String& systemId,
        const juce::String& startingNote,
        std::function<void (std::vector<PitchClass>)> onSuccess,
        ErrorCb onError = {});

    /**
     * GET /tuning-systems/{id}/{startingNote}/maqamat
     * Returns the list of maqamat available in a tuning system.
     */
    void fetchMaqamList (
        const juce::String& systemId,
        const juce::String& startingNote,
        std::function<void (std::vector<MaqamListEntry>)> onSuccess,
        ErrorCb onError = {});

    /**
     * GET /maqamat/{maqamId}?tuningSystem={id}&startingNote={note}&pitchClassDataType=all
     * Optionally with &transposeTo={tonicIdName} for transposed maqamat.
     * Returns ascending degree pitch classes with context-aware IPN references,
     * plus available transposition mapping (tonicId → transposition idName).
     */
    void fetchMaqamDetail (
        const juce::String& maqamId,
        const juce::String& systemId,
        const juce::String& startingNote,
        std::function<void (MaqamDetailResult)> onSuccess,
        ErrorCb onError = {},
        const juce::String& transpositionId = {});

    /** Run a generic job on the background thread (e.g. cache preloading). */
    void runOnThread (std::function<void()> job);

    /** Cancel any pending requests and jobs (does not cancel in-flight request). */
    void cancelPending();

private:
    // ── Internal request queue ────────────────────────────────────────────────
    struct Request
    {
        juce::URL url;
        std::function<void (const juce::var&)> onComplete;
        ErrorCb                                onError;
    };

    juce::CriticalSection  queueLock;
    std::queue<Request>    requestQueue;
    std::queue<std::function<void()>> jobQueue;

    void enqueue (Request req);
    void run() override;   // Thread::run — processes queue in background

    // Perform a single synchronous GET, parse JSON, return via callbacks on msg thread
    void performRequest (Request req);
};
