#include "AppUpdateChecker.h"
#include <juce_events/juce_events.h>      // MessageManager::callAsync
#include <juce_audio_processors/juce_audio_processors.h>   // JucePlugin_VersionString

namespace
{
    // Releases are published by .github/workflows/release.yml on tag push.
    constexpr const char* kLatestReleaseUrl =
        "https://api.github.com/repos/Music-Intelligence-Lab/tanghim/releases/latest";

    bool isAlive (const std::weak_ptr<std::atomic<bool>>& w)
    {
        if (auto sp = w.lock()) return sp->load();
        return true;   // no guard supplied — caller owns the lifetime
    }
}

AppUpdateChecker::AppUpdateChecker (std::weak_ptr<std::atomic<bool>> aliveFlag)
    : alive (std::move (aliveFlag))
{
    pool = std::make_unique<juce::ThreadPool> (juce::ThreadPoolOptions{}
                                                   .withNumberOfThreads (1)
                                                   .withThreadName ("Tanghim update check"));
}

AppUpdateChecker::~AppUpdateChecker()
{
    // Block until the in-flight check finishes so its lambda cannot outlive us.
    pool->removeAllJobs (true, 4000);
}

int AppUpdateChecker::compareVersions (const juce::String& a, const juce::String& b)
{
    auto parts = [] (juce::String v)
    {
        if (v.startsWithIgnoreCase ("v")) v = v.substring (1);
        juce::StringArray out;
        out.addTokens (v.upToFirstOccurrenceOf ("-", false, false), ".", {});
        return out;
    };

    const auto pa = parts (a), pb = parts (b);
    const int n = juce::jmax (pa.size(), pb.size());

    for (int i = 0; i < n; ++i)
    {
        const int va = i < pa.size() ? pa[i].getIntValue() : 0;
        const int vb = i < pb.size() ? pb[i].getIntValue() : 0;
        if (va != vb) return va < vb ? -1 : 1;
    }
    return 0;
}

bool AppUpdateChecker::check (std::function<void (juce::String, juce::String)> onUpdateAvailable,
                              std::function<void()>                            onUpToDate,
                              std::function<void (juce::String)>               onError)
{
    bool expected = false;
    if (! checking.compare_exchange_strong (expected, true))
        return false;   // already running

    pool->addJob ([this,
                   onUpdateAvailable = std::move (onUpdateAvailable),
                   onUpToDate        = std::move (onUpToDate),
                   onError           = std::move (onError)]
    {
        juce::String body, failure;

        {
            const juce::URL url { kLatestReleaseUrl };
            auto stream = url.createInputStream (
                juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs (10000)
                    // GitHub requires a User-Agent and rejects requests without one.
                    .withExtraHeaders ("Accept: application/vnd.github+json\r\n"
                                       "User-Agent: Tanghim"));

            if (stream != nullptr) body = stream->readEntireStreamAsString();
            else                   failure = "Could not reach GitHub";
        }

        juce::String newVersion, releaseUrl;

        if (failure.isEmpty())
        {
            const auto json = juce::JSON::parse (body);
            if (auto* obj = json.getDynamicObject())
            {
                const juce::String tag = obj->getProperty ("tag_name").toString();
                if (tag.isNotEmpty()
                    && compareVersions (tag, JucePlugin_VersionString) > 0)
                {
                    newVersion = tag.startsWithIgnoreCase ("v") ? tag.substring (1) : tag;
                    releaseUrl = obj->getProperty ("html_url").toString();
                }
            }
            else
            {
                failure = "Unexpected response from GitHub";
            }
        }

        checking.store (false, std::memory_order_relaxed);

        juce::MessageManager::callAsync (
            [this, newVersion, releaseUrl, failure,
             onUpdateAvailable, onUpToDate, onError]
            {
                if (! isAlive (alive)) return;

                if (failure.isNotEmpty())      { if (onError)            onError (failure); }
                else if (newVersion.isNotEmpty()) { if (onUpdateAvailable) onUpdateAvailable (newVersion, releaseUrl); }
                else                           { if (onUpToDate)         onUpToDate(); }
            });
    });

    return true;
}
