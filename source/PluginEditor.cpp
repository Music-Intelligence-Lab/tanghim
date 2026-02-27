#include "PluginEditor.h"
#include "NativeBridge.h"
#include "BuildTimestamp.h"
#include "model/PitchClass.h"
#include <map>

#if EMBED_UI_BUNDLE
  #include "BinaryData.h"
#endif

ArabicMaqamTunerEditor::ArabicMaqamTunerEditor (ArabicMaqamTunerProcessor& p)
    : AudioProcessorEditor (p), processor (p), midiDragButton (p)
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
    // Reserve 26px at bottom for native status bar (resized() ran before browser existed)
    browser->setBounds (getLocalBounds().withTrimmedBottom (26));

    // Native status bar components
    addAndMakeVisible (midiDragButton);
    midiDragButton.setVisible (false);

    addAndMakeVisible (updatesButton);
    updatesButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    updatesButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    updatesButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff808099));
    updatesButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffe8b339));
    updatesButton.onClick = [this] {
        processor.checkForDataUpdates (
            [] (auto) { /* onUpdatesFound - status shown via onStatusMessage */ },
            [] { /* onNoUpdates - status shown via onStatusMessage */ },
            [] (auto) { /* onError - status shown via onStatusMessage */ }
        );
    };

    addAndMakeVisible (clearCacheButton);
    clearCacheButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    clearCacheButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    clearCacheButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff808099));
    clearCacheButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffe8b339));
    clearCacheButton.onClick = [this] {
        processor.clearCache();

        // Visual feedback: flash button text, revert after 1.5s
        clearCacheButton.setButtonText (juce::CharPointer_UTF8 ("\xe2\x9c\x93 Cleared!"));
        clearCacheButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff4caf50));
        juce::Timer::callAfterDelay (1500, [safeThis = juce::Component::SafePointer (this)] {
            if (safeThis)
            {
                safeThis->clearCacheButton.setButtonText (juce::CharPointer_UTF8 ("\xc3\x97 Clear Cache"));
                safeThis->clearCacheButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff808099));
            }
        });
    };

    // Setup MIDI preset trigger controls
    setupMidiPresetControls();

    // Listen for MIDI device connect/disconnect events (cross-platform: CoreMIDI/WinMM/ALSA)
    midiDeviceListConnection = juce::MidiDeviceListConnection::make ([this]
    {
        midiDevicesChanged.store (true, std::memory_order_relaxed);
    });

    // Set processor change callbacks
    processor.onTuningStateChanged   = [this] { emitTuningStateChanged(); };
    processor.onTuningSystemsLoaded  = [this] { emitTuningSystemsLoaded(); };
    processor.onMaqamListLoaded      = [this] { emitMaqamListLoaded(); };
    processor.onStatusMessage        = [this] (juce::String msg) { emitStatusMessage (msg); };
    processor.onSlotCentsChanged     = [this] (int idx, double cents) { emitSlotCentsChanged (idx, cents); };

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
    midiDeviceListConnection.reset();
    // Reset LookAndFeel before components are destroyed
    midiDeviceSelector.setLookAndFeel (nullptr);
    midiChannelSelector.setLookAndFeel (nullptr);
    processor.onTuningStateChanged  = {};
    processor.onTuningSystemsLoaded = {};
    processor.onMaqamListLoaded     = {};
    processor.onStatusMessage       = {};
    processor.onSlotCentsChanged    = {};
}

void ArabicMaqamTunerEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e)); // dark fallback while WebView loads

    // Native status bar background (26px at bottom)
    const int nativeStatusBarHeight = 26;
    auto statusBarBounds = getLocalBounds().removeFromBottom (nativeStatusBarHeight);
    g.setColour (juce::Colour (0xff16162b)); // --surface2 equivalent
    g.fillRect (statusBarBounds);
    g.setColour (juce::Colour (0xff2d2d4a)); // --border equivalent
    g.drawHorizontalLine (statusBarBounds.getY(), 0.0f, static_cast<float> (getWidth()));

    // Version + timestamp label (left side)
    g.setColour (juce::Colour (0xff808099)); // --text-muted equivalent
    g.setFont (11.0f);
    juce::String versionText = "v" + juce::String (PLUGIN_VERSION) + " (" + juce::String (BUILD_TIMESTAMP) + ")";
    g.drawText (versionText,
                statusBarBounds.withTrimmedLeft (16).withWidth (200),
                juce::Justification::centredLeft);
}

void ArabicMaqamTunerEditor::resized()
{
    // Reserve 26px at bottom for native status bar (MIDI button overlay doesn't work on WKWebView)
    const int nativeStatusBarHeight = 26;

    if (browser)
        browser->setBounds (getLocalBounds().withTrimmedBottom (nativeStatusBarHeight));

    // Position native status bar components (right to left)
    const int btnHeight = 18;
    const int yPos = getHeight() - nativeStatusBarHeight + (nativeStatusBarHeight - btnHeight) / 2;
    const int rightMargin = 16;
    int rightEdge = getWidth() - rightMargin;

    // Updates button (rightmost)
    const int updatesBtnWidth = 70;
    updatesButton.setBounds (rightEdge - updatesBtnWidth, yPos, updatesBtnWidth, btnHeight);
    rightEdge -= updatesBtnWidth + 4;

    // Clear Cache button (to the left of Updates)
    const int clearCacheBtnWidth = 80;
    clearCacheButton.setBounds (rightEdge - clearCacheBtnWidth, yPos, clearCacheBtnWidth, btnHeight);
    rightEdge -= clearCacheBtnWidth + 8;

    // MIDI drag button (to the left of Clear Cache)
    const int midiBtnWidth = 42;
    midiDragButton.setBounds (rightEdge - midiBtnWidth, yPos, midiBtnWidth, btnHeight);
    rightEdge -= midiBtnWidth + 12;

    // MIDI preset configuration (to the left of MIDI button)
    // "Preset MIDI: [device v] [ch v]"
    const int chSelectorWidth = 50;
    midiChannelSelector.setBounds (rightEdge - chSelectorWidth, yPos, chSelectorWidth, btnHeight);
    rightEdge -= chSelectorWidth + 4;

    const int deviceSelectorWidth = 120;
    midiDeviceSelector.setBounds (rightEdge - deviceSelectorWidth, yPos, deviceSelectorWidth, btnHeight);
    rightEdge -= deviceSelectorWidth + 4;

    const int prefixWidth = 130;
    midiPresetLabel.setBounds (rightEdge - prefixWidth, yPos, prefixWidth, btnHeight);
}

// ── MIDI activity + MTS-ESP status polling ───────────────────────────────────

void ArabicMaqamTunerEditor::timerCallback()
{
    if (! browser) return;

    // ── MIDI-triggered preset (~30Hz check) ───────────────────────────────
    {
        const int presetIdx = processor.consumePendingMidiPreset();
        if (presetIdx >= 0 && presetIdx < 16)
        {
            processor.applyPreset (presetIdx);
            emitTuningStateChanged();
        }
    }

    // ── Ref freq automation dirty flag (~30Hz) ──────────────────────────
    if (processor.refFreqAutomationDirty.exchange (false, std::memory_order_relaxed))
        emitTuningStateChanged();

    // ── Slot automation dirty flags (~30Hz) ──────────────────────────
    {
        const uint16_t dirtyMask = processor.slotAutomationDirtyMask.exchange (
            0, std::memory_order_relaxed);
        if (dirtyMask != 0)
        {
            const auto& state = processor.getActiveTuningState();
            for (int i = 0; i < 12; ++i)
            {
                if (dirtyMask & (1u << i))
                    emitSlotCentsChanged (i, state.slots[(size_t) i].centsOffset);
            }
        }
    }

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

    // ── MIDI drag button visibility + MTS-ESP status polling (~2Hz) ──────────
    if (++mtsStatusFrameCounter >= 15)
    {
        mtsStatusFrameCounter = 0;

        // Update MIDI drag button visibility based on maqam selection
        const auto maqamId = processor.getCurrentMaqamId();
        if (maqamId != lastMaqamId)
        {
            lastMaqamId = maqamId;
            midiDragButton.setVisible (maqamId.isNotEmpty());
        }

        // MTS-ESP client count is a cheap shared-memory read — keep at 2Hz
        const int  totalReceivers   = processor.mtsNumReceivers();
        const bool isMtsTransmitter = processor.isMtsTransmitter();
        bool       mtsStatusDirty   = (totalReceivers != lastMtsTotal
                                        || isMtsTransmitter != lastIsMtsTransmitter);

        // ── Event-driven MIDI device list refresh ─────────────────────
        if (midiDevicesChanged.exchange (false, std::memory_order_relaxed))
        {
            // Detect stale connections (device unplugged while input was open)
            processor.recheckMidiPresetDevice();

            populateMidiDeviceList();

            // Retry opening device if configured but not yet connected
            const auto deviceName = processor.getMidiPresetDevice();
            if (deviceName.isNotEmpty() && ! processor.isMidiPresetDeviceOpen())
                processor.setMidiPresetDevice (deviceName);
        }

        // ── Slow poll: filesystem I/O (~every 5 seconds) ────────────
        if (++slowPollCounter >= 10)
        {
            slowPollCounter = 0;

            // ReceiverRegistry::scan() — directory iteration + mtime checks
            const auto receiverCounts = processor.getReceiverCounts();
            if (receiverCounts != lastReceiverCounts)
            {
                lastReceiverCounts = receiverCounts;
                mtsStatusDirty = true;
            }

            // Periodic stale file cleanup (~every 60 seconds)
            if (++staleCleanupCounter >= 12)
            {
                staleCleanupCounter = 0;
                ReceiverRegistry::cleanStale (10.0);
            }
        }

        // Emit MTS status if changed
        if (mtsStatusDirty)
        {
            lastMtsTotal         = totalReceivers;
            lastIsMtsTransmitter = isMtsTransmitter;

            const int tanghimTotal = lastReceiverCounts.mpeReceivers + lastReceiverCounts.monoPbReceivers;
            const int mtsNative    = std::max (0, totalReceivers - tanghimTotal);

            auto* obj = new juce::DynamicObject();
            obj->setProperty ("isMtsTransmitter", isMtsTransmitter);
            obj->setProperty ("mtsNativeCount",   mtsNative);
            obj->setProperty ("mpeCount",         lastReceiverCounts.mpeReceivers);
            obj->setProperty ("monoPbCount",      lastReceiverCounts.monoPbReceivers);
            browser->emitEventIfBrowserIsVisible ("mtsStatusChanged", juce::var (obj));
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
    // Forward to React (for any future use)
    if (browser) browser->emitEventIfBrowserIsVisible ("statusMessage", juce::var (msg));
}

void ArabicMaqamTunerEditor::emitSlotCentsChanged (int chromaticIndex, double centsOffset)
{
    if (! browser) return;
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("index", chromaticIndex);
    obj->setProperty ("cents", centsOffset);
    browser->emitEventIfBrowserIsVisible ("slotCentsChanged", juce::var (obj));
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

// ── MidiDragButton implementation ─────────────────────────────────────────────

void MidiDragButton::prepareMidiFile()
{
    tempMidiFile = juce::File();

    const juce::String maqamId = processor.getCurrentMaqamId();
    if (maqamId.isEmpty()) return;

    const auto& degreeNames = processor.getCurrentDegreeNames();
    if (degreeNames.empty()) return;

    // Build paoNameMap
    std::map<juce::String, int> paoNameMap;
    const auto& allPCs = processor.getCurrentPitchClasses();
    for (const auto& pc : allPCs)
    {
        if (pc.noteName.isEmpty()) continue;
        const int ci = chromaticIndexForIpnRef (pc.ipnReference);
        if (ci >= 0) paoNameMap[pc.noteName] = ci;
    }

    // Find tonic
    const juce::String tonicName = degreeNames[0];
    auto tonicIt = paoNameMap.find (tonicName);
    if (tonicIt == paoNameMap.end()) return;
    const int tonicChromaticIdx = tonicIt->second;

    int tonicMidi = -1;
    for (const auto& pc : allPCs)
    {
        if (pc.noteName == tonicName && pc.midiNoteNumber >= 48 && pc.midiNoteNumber < 60)
        {
            tonicMidi = pc.midiNoteNumber;
            break;
        }
    }
    if (tonicMidi < 0)
    {
        for (const auto& pc : allPCs)
        {
            if (pc.noteName == tonicName)
            {
                tonicMidi = pc.midiNoteNumber;
                break;
            }
        }
    }
    if (tonicMidi < 0) return;

    // Build MIDI notes
    std::vector<int> midiNotes;
    for (const auto& degreeName : degreeNames)
    {
        auto it = paoNameMap.find (degreeName);
        if (it == paoNameMap.end()) continue;
        int interval = it->second - tonicChromaticIdx;
        if (interval < 0) interval += 12;
        int midiNote = tonicMidi + interval;
        if (midiNote >= 0 && midiNote <= 127)
            midiNotes.push_back (midiNote);
    }
    if (midiNotes.empty()) return;

    // Build maqam info
    MidiFileGenerator::MaqamInfo info;
    info.maqamDisplay    = processor.getCurrentMaqamDisplay();
    info.tonicPaoDisplay = processor.getCurrentTonicDisplay();
    info.tonicIpn        = processor.getCurrentTonicEnglish();
    info.tonicSolfege    = processor.getCurrentTonicSolfege();
    info.midiNotes       = midiNotes;

    if (info.maqamDisplay.isEmpty()) info.maqamDisplay = maqamId;
    if (info.tonicPaoDisplay.isEmpty()) info.tonicPaoDisplay = tonicName;
    if (info.tonicIpn.isEmpty())
    {
        static const char* IPN_NAMES[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        info.tonicIpn = juce::String (IPN_NAMES[tonicMidi % 12]) + juce::String ((tonicMidi / 12) - 1);
    }
    if (info.tonicSolfege.isEmpty())
    {
        for (const auto& pc : allPCs)
        {
            if (pc.midiNoteNumber == tonicMidi)
            {
                info.tonicSolfege = pc.solfege;
                break;
            }
        }
    }

    // Generate and write to temp file
    auto midiData = MidiFileGenerator::generate (info);
    auto filename = MidiFileGenerator::buildFilename (info);

    // Use ~/Library/Caches/Tanghim/ — accessible to Finder for drag-to-filesystem
    auto tempDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("Tanghim").getChildFile ("midi-export");
    tempDir.createDirectory();

    tempMidiFile = tempDir.getChildFile (filename);
    bool writeSuccess = tempMidiFile.replaceWithData (midiData.data(), midiData.size());

    DBG ("MIDI file: " << tempMidiFile.getFullPathName()
         << " | size=" << midiData.size()
         << " | write=" << (writeSuccess ? "OK" : "FAILED")
         << " | exists=" << (tempMidiFile.existsAsFile() ? "YES" : "NO"));
}

// ── MIDI preset trigger control setup ──────────────────────────────────────

void ArabicMaqamTunerEditor::setupMidiPresetControls()
{
    // Consistent dark theme colors
    const auto bgColor      = juce::Colour (0xff1a1a2e);  // --surface
    const auto borderColor  = juce::Colour (0xff2d2d4a);  // --border
    const auto textColor    = juce::Colour (0xffe94560);  // red accent (matches inactive presets)
    const auto mutedColor   = juce::Colour (0xff808099);  // --text-muted

    // Static prefix label
    addAndMakeVisible (midiPresetLabel);
    midiPresetLabel.setColour (juce::Label::textColourId, mutedColor);
    midiPresetLabel.setFont (juce::FontOptions (11.0f));

    // MIDI device selector dropdown
    addAndMakeVisible (midiDeviceSelector);
    midiDeviceSelector.setLookAndFeel (&statusBarLnF);
    midiDeviceSelector.setColour (juce::ComboBox::backgroundColourId, bgColor);
    midiDeviceSelector.setColour (juce::ComboBox::textColourId, textColor);
    midiDeviceSelector.setColour (juce::ComboBox::outlineColourId, borderColor);
    midiDeviceSelector.setColour (juce::ComboBox::arrowColourId, textColor);
    midiDeviceSelector.onChange = [this] { onMidiDeviceChanged(); };
    populateMidiDeviceList();

    // MIDI channel selector dropdown
    addAndMakeVisible (midiChannelSelector);
    midiChannelSelector.setLookAndFeel (&statusBarLnF);
    midiChannelSelector.setColour (juce::ComboBox::backgroundColourId, bgColor);
    midiChannelSelector.setColour (juce::ComboBox::textColourId, textColor);
    midiChannelSelector.setColour (juce::ComboBox::outlineColourId, borderColor);
    midiChannelSelector.setColour (juce::ComboBox::arrowColourId, textColor);
    midiChannelSelector.onChange = [this] { onMidiChannelChanged(); };
    populateMidiChannelList();
}

void ArabicMaqamTunerEditor::populateMidiChannelList()
{
    midiChannelSelector.clear();
    midiChannelSelector.addItem ("All", 1);  // ID 1 = channel 0 (any)
    for (int ch = 1; ch <= 16; ++ch)
        midiChannelSelector.addItem (juce::String (ch), ch + 1);  // ID 2-17 = channels 1-16

    // Select current channel
    const int channel = processor.getMidiPresetChannel();
    midiChannelSelector.setSelectedId (channel + 1);  // channel 0 → ID 1, channel 1 → ID 2, etc.
}

void ArabicMaqamTunerEditor::onMidiChannelChanged()
{
    const int selectedId = midiChannelSelector.getSelectedId();
    const int channel = selectedId - 1;  // ID 1 → 0 (All), ID 2 → 1, etc.
    processor.setMidiPresetChannel (channel);
}

void ArabicMaqamTunerEditor::populateMidiDeviceList()
{
    midiDeviceSelector.clear (juce::dontSendNotification);
    const auto devices = processor.getAvailableMidiDevices();
    for (int i = 0; i < devices.size(); ++i)
        midiDeviceSelector.addItem (devices[i], i + 1);

    // Select current device or "None" — use dontSendNotification to avoid
    // triggering onChange (which would close+reopen the device or erase the
    // saved device name if the device isn't found yet at editor construction)
    const auto currentDevice = processor.getMidiPresetDevice();
    if (currentDevice.isEmpty())
    {
        midiDeviceSelector.setSelectedId (1, juce::dontSendNotification);  // "None"
    }
    else
    {
        const int idx = devices.indexOf (currentDevice);
        if (idx >= 0)
            midiDeviceSelector.setSelectedId (idx + 1, juce::dontSendNotification);
        else
            midiDeviceSelector.setSelectedId (1, juce::dontSendNotification);  // "None" if device not found yet
    }
}

void ArabicMaqamTunerEditor::onMidiDeviceChanged()
{
    const int selected = midiDeviceSelector.getSelectedId();
    if (selected <= 1)
    {
        processor.setMidiPresetDevice ("");  // None / disabled
    }
    else
    {
        const auto devices = processor.getAvailableMidiDevices();
        if (selected - 1 < devices.size())
            processor.setMidiPresetDevice (devices[selected - 1]);
    }

    processor.saveSettingsToDisk();
}
