#include "PluginEditor.h"
#include "NativeBridge.h"

#if EMBED_UI_BUNDLE
  #include "BinaryData.h"
#endif

ArabicMaqamTunerEditor::ArabicMaqamTunerEditor (ArabicMaqamTunerProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setSize (832, 620);
    setResizable (true, true);
    setResizeLimits (832, 620, 2400, 4000);

    // Create bridge first so we can register native functions in Options
    bridge = std::make_unique<NativeBridge> (processor);

    // Build WebBrowserComponent options
    juce::WebBrowserComponent::Options opts;
    opts = opts.withNativeIntegrationEnabled();

    // Register all C++ ↔ JS native functions via bridge
    opts = bridge->applyTo (opts);

#if defined(JUCE_WINDOWS)
    opts = opts.withBackend (juce::WebBrowserComponent::Options::Backend::webview2)
               .withWinWebView2Options (
                   juce::WebBrowserComponent::Options::WinWebView2Options()
                       .withUserDataFolder (
                           juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                               .getChildFile ("Tanghim/webview")));
#endif

#if EMBED_UI_BUNDLE
    // Release: serve bundled dist/ from BinaryData
    opts = opts.withResourceProvider (
        [this] (const juce::String& urlStr) -> std::optional<juce::WebBrowserComponent::Resource>
        {
            const auto path = juce::URL (urlStr).getSubPath();
            auto resource = getResourceForPath (path);
            if (resource.mimeType.isNotEmpty())
                return resource;
            return {};
        },
        juce::URL (juce::WebBrowserComponent::getResourceProviderRoot())
    );
#endif

    browser = std::make_unique<juce::WebBrowserComponent> (opts);
    addAndMakeVisible (*browser);
    browser->setBounds (getLocalBounds());  // initial layout (resized() ran before browser existed)

    // Set processor change callbacks
    processor.onTuningStateChanged   = [this] { emitTuningStateChanged(); };
    processor.onTuningSystemsLoaded  = [this] { emitTuningSystemsLoaded(); };
    processor.onMaqamListLoaded      = [this] { emitMaqamListLoaded(); };
    processor.onStatusMessage        = [this] (juce::String msg) { emitStatusMessage (msg); };

#if EMBED_UI_BUNDLE
    browser->goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
#else
    // Debug: hot-reload from Vite dev server
    browser->goToURL ("http://localhost:5173");
#endif

    // Poll MIDI activity at ~30 fps for UI feedback
    startTimerHz (30);
}

ArabicMaqamTunerEditor::~ArabicMaqamTunerEditor()
{
    stopTimer();
    processor.onTuningStateChanged  = {};
    processor.onTuningSystemsLoaded = {};
    processor.onMaqamListLoaded     = {};
    processor.onStatusMessage       = {};
}

void ArabicMaqamTunerEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e)); // dark fallback while WebView loads
}

void ArabicMaqamTunerEditor::resized()
{
    if (browser) browser->setBounds (getLocalBounds());
}

// ── MIDI activity + MTS-ESP status polling ───────────────────────────────────

void ArabicMaqamTunerEditor::timerCallback()
{
    if (! browser) return;

    // ── MIDI activity (~30Hz) ─────────────────────────────────────────────
    {
        uint32_t ons[4], offs[4];
        bool any = false;
        for (int i = 0; i < 4; ++i)
        {
            ons[i]  = processor.noteOnBits[i].exchange (0, std::memory_order_relaxed);
            offs[i] = processor.noteOffBits[i].exchange (0, std::memory_order_relaxed);
            if (ons[i] | offs[i]) any = true;
        }
        if (any)
        {
            juce::Array<juce::var> onArr, offArr;
            for (int w = 0; w < 4; ++w)
            {
                for (int b = 0; b < 32; ++b)
                {
                    const int note = w * 32 + b;
                    if (ons[w]  & (1u << b)) onArr.add (note);
                    if (offs[w] & (1u << b)) offArr.add (note);
                }
            }
            auto* obj = new juce::DynamicObject();
            obj->setProperty ("on",  juce::var (onArr));
            obj->setProperty ("off", juce::var (offArr));
            browser->emitEventIfBrowserIsVisible ("midiActivity", juce::var (obj));
        }
    }

    // ── MTS-ESP status polling (~2Hz) ─────────────────────────────────────
    if (++mtsStatusFrameCounter >= 15)
    {
        mtsStatusFrameCounter = 0;

        const int  totalReceivers   = processor.mtsNumReceivers();
        const bool isMtsTransmitter = processor.isMtsTransmitter();
        const auto receiverCounts   = processor.getReceiverCounts();

        if (totalReceivers != lastMtsTotal
            || isMtsTransmitter != lastIsMtsTransmitter
            || receiverCounts != lastReceiverCounts)
        {
            lastMtsTotal          = totalReceivers;
            lastIsMtsTransmitter  = isMtsTransmitter;
            lastReceiverCounts    = receiverCounts;

            const int tanghimTotal = receiverCounts.mpeReceivers + receiverCounts.monoPbReceivers;
            const int mtsNative    = std::max (0, totalReceivers - tanghimTotal);

            auto* obj = new juce::DynamicObject();
            obj->setProperty ("isMtsTransmitter", isMtsTransmitter);
            obj->setProperty ("mtsNativeCount",   mtsNative);
            obj->setProperty ("mpeCount",         receiverCounts.mpeReceivers);
            obj->setProperty ("monoPbCount",      receiverCounts.monoPbReceivers);
            browser->emitEventIfBrowserIsVisible ("mtsStatusChanged", juce::var (obj));
        }

        // Periodic stale file cleanup (~every 30 seconds)
        if (++staleCleanupCounter >= 60)
        {
            staleCleanupCounter = 0;
            ReceiverRegistry::cleanStale (10.0);
        }
    }
}

// ── Push state to WebView ─────────────────────────────────────────────────────

void ArabicMaqamTunerEditor::emitTuningStateChanged()
{
    if (browser) browser->emitEventIfBrowserIsVisible ("tuningStateChanged",
                                                        bridge->buildTuningStateJson());
}

void ArabicMaqamTunerEditor::emitTuningSystemsLoaded()
{
    if (browser) browser->emitEventIfBrowserIsVisible ("tuningSystemsLoaded",
                                                        bridge->buildTuningSystemsJson());
}

void ArabicMaqamTunerEditor::emitMaqamListLoaded()
{
    if (browser) browser->emitEventIfBrowserIsVisible ("maqamListLoaded",
                                                        bridge->buildMaqamListJson());
}

void ArabicMaqamTunerEditor::emitStatusMessage (const juce::String& msg)
{
    if (browser) browser->emitEventIfBrowserIsVisible ("statusMessage", juce::var (msg));
}

// ── Resource provider (release builds) ───────────────────────────────────────

#if EMBED_UI_BUNDLE
juce::WebBrowserComponent::Resource ArabicMaqamTunerEditor::getResourceForPath (const juce::String& path)
{
    auto cleanPath = path.isEmpty() || path == "/" ? juce::String ("index.html") : path.trimCharactersAtStart ("/");

    // Look up in BinaryData
    int size = 0;
    const char* data = BinaryData::getNamedResource (
        cleanPath.replaceCharacters ("./- ", "____").toRawUTF8(), size);

    if (data == nullptr)
    {
        // Fallback to index.html for SPA routing
        data = BinaryData::getNamedResource ("index_html", size);
        cleanPath = "index.html";
    }

    if (data == nullptr) return {};

    // Determine MIME type
    juce::String mime = "text/plain";
    if      (cleanPath.endsWith (".html")) mime = "text/html";
    else if (cleanPath.endsWith (".js"))   mime = "application/javascript";
    else if (cleanPath.endsWith (".css"))  mime = "text/css";
    else if (cleanPath.endsWith (".json")) mime = "application/json";
    else if (cleanPath.endsWith (".svg"))  mime = "image/svg+xml";
    else if (cleanPath.endsWith (".png"))  mime = "image/png";
    else if (cleanPath.endsWith (".woff2")) mime = "font/woff2";

    return { std::vector<std::byte> (reinterpret_cast<const std::byte*> (data),
                                     reinterpret_cast<const std::byte*> (data) + size),
             mime };
}
#else
juce::WebBrowserComponent::Resource ArabicMaqamTunerEditor::getResourceForPath (const juce::String&)
{
    return {};
}
#endif
