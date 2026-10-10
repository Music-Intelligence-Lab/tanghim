#pragma once
#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <memory>

/**
 * Checks GitHub Releases for a newer Tanghīm build.
 *
 * This is the APPLICATION version check — distinct from DataUpdateChecker, which
 * tracks tuning-data versions on the DiArMaqAr API. The two are reported separately
 * in the status bar because they mean different things to the user: one is "new
 * musicological data is available", the other is "a new plug-in is available".
 *
 * Notify-only by design. Tanghīm never replaces its own binaries: plug-in files are
 * memory-mapped by the host while loaded, and the installers need elevated privileges,
 * so a self-replacing update could corrupt a running install. We surface the release
 * and let the user quit their DAW and run the installer. See RELEASING.md.
 */
class AppUpdateChecker
{
public:
    explicit AppUpdateChecker (std::weak_ptr<std::atomic<bool>> alive = {});
    ~AppUpdateChecker();

    /** Async check. Callbacks fire on the message thread.
     *  onUpdateAvailable receives the new version ("0.9.2") and its release page URL.
     *  @return false if a check is already running (no callback will fire). */
    bool check (std::function<void (juce::String version, juce::String url)> onUpdateAvailable,
                std::function<void()>                                        onUpToDate = {},
                std::function<void (juce::String)>                           onError    = {});

    bool isChecking() const { return checking.load (std::memory_order_relaxed); }

    /** Compare dotted numeric versions ("0.9.10" > "0.9.9"). Returns >0 if a > b,
        0 if equal, <0 if a < b. A leading "v" is ignored. Non-numeric or missing
        components count as 0, so "1.0" == "1.0.0". */
    static int compareVersions (const juce::String& a, const juce::String& b);

private:
    std::weak_ptr<std::atomic<bool>> alive;
    std::atomic<bool> checking { false };
    std::unique_ptr<juce::ThreadPool> pool;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppUpdateChecker)
};
