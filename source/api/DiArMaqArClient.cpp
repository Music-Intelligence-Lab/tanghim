#include "DiArMaqArClient.h"
#include "ApiResponseParser.h"

DiArMaqArClient::DiArMaqArClient()
    : juce::Thread ("DiArMaqArClientThread")
{
    startThread (juce::Thread::Priority::low);
}

DiArMaqArClient::~DiArMaqArClient()
{
    cancelPending();
    signalThreadShouldExit();
    notify();
    stopThread (15000);
}

// ── Public API ────────────────────────────────────────────────────────────────

void DiArMaqArClient::fetchTuningSystems (
    std::function<void (std::vector<TuningSystem>)> onSuccess,
    ErrorCb onError)
{
    const juce::URL url { juce::String (BASE_URL) + "/tuning-systems" };

    enqueue ({
        url,
        [onSuccess = std::move (onSuccess)] (const juce::var& json)
        {
            auto systems = ApiResponseParser::parseTuningSystemsList (json);
            juce::MessageManager::callAsync ([onSuccess, systems = std::move(systems)] () mutable
            {
                onSuccess (std::move (systems));
            });
        },
        std::move (onError)
    });
}

void DiArMaqArClient::fetchPitchClasses (
    const juce::String& systemId,
    const juce::String& startingNote,
    std::function<void (std::vector<PitchClass>)> onSuccess,
    ErrorCb onError)
{
    const juce::URL url {
        juce::String (BASE_URL) + "/tuning-systems/" + systemId + "/" + startingNote
        + "/pitch-classes?pitchClassDataType=all"
    };

    enqueue ({
        url,
        [onSuccess = std::move (onSuccess)] (const juce::var& json)
        {
            auto pcs = ApiResponseParser::parsePitchClasses (json);
            juce::MessageManager::callAsync ([onSuccess, pcs = std::move(pcs)] () mutable
            {
                onSuccess (std::move (pcs));
            });
        },
        std::move (onError)
    });
}

void DiArMaqArClient::fetchMaqamList (
    const juce::String& systemId,
    const juce::String& startingNote,
    std::function<void (std::vector<MaqamListEntry>)> onSuccess,
    ErrorCb onError)
{
    const juce::URL url {
        juce::String (BASE_URL) + "/tuning-systems/" + systemId + "/" + startingNote
        + "/maqamat?includeMaqamDegrees=true&includeTranspositions=true&includeDegreeDetails=true"
    };

    enqueue ({
        url,
        [onSuccess = std::move (onSuccess)] (const juce::var& json)
        {
            auto entries = ApiResponseParser::parseMaqamList (json);
            juce::MessageManager::callAsync ([onSuccess, entries = std::move(entries)] () mutable
            {
                onSuccess (std::move (entries));
            });
        },
        std::move (onError)
    });
}

void DiArMaqArClient::runOnThread (std::function<void()> job)
{
    {
        juce::ScopedLock sl (queueLock);
        jobQueue.push (std::move (job));
    }
    notify();
}

void DiArMaqArClient::cancelPending()
{
    juce::ScopedLock sl (queueLock);
    while (! requestQueue.empty())
        requestQueue.pop();
    while (! jobQueue.empty())
        jobQueue.pop();
}

// ── Background thread ─────────────────────────────────────────────────────────

void DiArMaqArClient::enqueue (Request req)
{
    {
        juce::ScopedLock sl (queueLock);
        requestQueue.push (std::move (req));
    }
    notify(); // wake the background thread
}

void DiArMaqArClient::run()
{
    while (! threadShouldExit())
    {
        // Wait for a request/job or exit signal
        wait (-1);

        while (! threadShouldExit())
        {
            // Drain generic jobs first (e.g. cache preloading)
            std::function<void()> job;
            {
                juce::ScopedLock sl (queueLock);
                if (! jobQueue.empty())
                {
                    job = std::move (jobQueue.front());
                    jobQueue.pop();
                }
            }
            if (job)
            {
                job();
                continue;
            }

            // Then drain HTTP requests
            Request req;
            {
                juce::ScopedLock sl (queueLock);
                if (requestQueue.empty()) break;
                req = std::move (requestQueue.front());
                requestQueue.pop();
            }
            performRequest (std::move (req));
        }
    }
}

void DiArMaqArClient::performRequest (Request req)
{
    if (threadShouldExit()) return;

    juce::String errorMsg;

    {
        int statusCode = 0;
        juce::StringPairArray headers;
        headers.set ("Accept", "application/json");

        const auto stream = req.url.createInputStream (
            juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                .withConnectionTimeoutMs (10000)
                .withExtraHeaders ("Accept: application/json")
                .withHttpRequestCmd ("GET")
                .withStatusCode (&statusCode));

        if (threadShouldExit()) return;

        if (stream != nullptr && statusCode == 200)
        {
            const juce::String body = stream->readEntireStreamAsString();
            const juce::var json = juce::JSON::parse (body);

            if (! json.isVoid())
            {
                req.onComplete (json);
                return;
            }
            errorMsg = "Invalid JSON in response";
        }
        else if (stream == nullptr)
        {
            errorMsg = "No connection (check network)";
        }
        else
        {
            errorMsg = "HTTP " + juce::String (statusCode);
        }
    }

    if (req.onError)
    {
        auto onError = req.onError;
        juce::MessageManager::callAsync ([onError, errorMsg] ()
        {
            onError (errorMsg);
        });
    }
}
