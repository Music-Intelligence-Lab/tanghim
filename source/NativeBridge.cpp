#include "NativeBridge.h"
#include "BuildTimestamp.h"
#include "engine/MidiFileGenerator.h"

NativeBridge::NativeBridge (ArabicMaqamTunerProcessor& p)
    : processor (p)
{
}

// ── Options builder ───────────────────────────────────────────────────────────

juce::WebBrowserComponent::Options NativeBridge::applyTo (juce::WebBrowserComponent::Options opts) const
{
    using Completion = juce::WebBrowserComponent::NativeFunctionCompletion;

    // ── getTuningSystems ──────────────────────────────────────────────────────
    opts = opts.withNativeFunction ("getTuningSystems",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            complete (buildTuningSystemsJson());
        });

    // ── selectTuningSystem(systemId, startingNote) ────────────────────────────
    opts = opts.withNativeFunction ("selectTuningSystem",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            const juce::String systemId    = args.size() > 0 ? args[0].toString() : "";
            const juce::String startingNote = args.size() > 1 ? args[1].toString() : "";
            processor.loadTuningSystem (systemId, startingNote,
                [complete] () { complete (juce::var (true)); });
        });

    // ── setSliderVariant(chromaticIndex, variantIndex) ────────────────────────
    opts = opts.withNativeFunction ("setSliderVariant",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 2)
                processor.setSliderVariant ((int) args[0], (int) args[1]);
            complete (buildTuningStateJson());
        });

    // ── setNoteVariant(midiNote, variantIndex) ─────────────────────────────
    opts = opts.withNativeFunction ("setNoteVariant",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 2)
                processor.setNoteVariant ((int) args[0], (int) args[1]);
            complete (buildTuningStateJson());
        });

    // ── setSlotCents(chromaticIndex, centsValue) ────────────────────────────
    // Fire-and-forget during continuous drag — updates MTS-ESP immediately
    // but does NOT push full state JSON back (too heavy at ~60fps).
    opts = opts.withNativeFunction ("setSlotCents",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 2)
                processor.setSlotCents ((int) args[0], (double) args[1]);
            complete (juce::var());
        });

    // ── setSlotCentsFinalize(chromaticIndex, centsValue) ────────────────────
    // Called on mouseup — updates tuning + returns full state for WebView sync.
    opts = opts.withNativeFunction ("setSlotCentsFinalize",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 2)
                processor.finalizeSlotCents ((int) args[0], (double) args[1]);
            complete (buildTuningStateJson());
        });

    // ── applyPreset(presetIndex) ──────────────────────────────────────────────
    opts = opts.withNativeFunction ("applyPreset",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 1)
                processor.applyPreset ((int) args[0]);
            complete (buildTuningStateJson());
        });

    // ── assignPreset(index, maqamId, maqamDisplay, baseMaqamId, isTransposed,
    //                 tonicNote, tonicIpn, tonicSolfege, setIndex, [sliderPos x12], ...) ────────
    opts = opts.withNativeFunction ("assignPreset",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            const int idx                   = args.size() > 0 ? (int) args[0] : 0;
            const juce::String id           = args.size() > 1 ? args[1].toString() : "";
            const juce::String disp         = args.size() > 2 ? args[2].toString() : "";
            const juce::String baseId       = args.size() > 3 ? args[3].toString() : "";
            const bool transposed           = args.size() > 4 ? (bool) args[4] : false;
            const juce::String tonicN       = args.size() > 5 ? args[5].toString() : "";
            const juce::String tonicIpn     = args.size() > 6 ? args[6].toString() : "";
            const juce::String tonicSolfege = args.size() > 7 ? args[7].toString() : "";
            const int setIdx                = args.size() > 8 ? (int) args[8] : -1;

            std::array<int, 12> positions;
            positions.fill (0);
            if (args.size() > 9 && args[9].isArray())
                for (int i = 0; i < 12 && i < args[9].size(); ++i)
                    positions[(size_t) i] = (int) args[9][i];

            std::vector<juce::String> degreeNames;
            if (args.size() > 10 && args[10].isArray())
                for (int i = 0; i < args[10].size(); ++i)
                    degreeNames.push_back (args[10][i].toString());

            std::array<double, 12> centsOffsets;
            centsOffsets.fill (0.0);
            if (args.size() > 11 && args[11].isArray())
                for (int i = 0; i < 12 && i < args[11].size(); ++i)
                    centsOffsets[(size_t) i] = (double) args[11][i];

            const juce::String tuningSystemId = args.size() > 12 ? args[12].toString() : "";
            const juce::String startingNote   = args.size() > 13 ? args[13].toString() : "";

            processor.assignPreset (idx, id, disp, baseId, transposed, tonicN, tonicIpn, tonicSolfege, setIdx,
                                    positions, degreeNames, centsOffsets, tuningSystemId, startingNote);
            complete (buildPresetsJson());
        });

    // ── clearPreset(index) ────────────────────────────────────────────────────
    opts = opts.withNativeFunction ("clearPreset",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 1)
                processor.clearPreset ((int) args[0]);
            complete (buildPresetsJson());
        });

    // ── getMaqamList() ─────────────────────────────────────────────────────────
    opts = opts.withNativeFunction ("getMaqamList",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            complete (buildMaqamListJson());
        });

    // ── applyMaqam(maqamId, transpositionIndex) ──────────────────────────────
    opts = opts.withNativeFunction ("applyMaqam",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            const juce::String maqamId = args.size() > 0 ? args[0].toString() : "";
            const int transIdx         = args.size() > 1 ? (int) args[1] : -1;
            if (maqamId.isNotEmpty())
                processor.applyMaqam (maqamId, transIdx);
            complete (buildTuningStateJson());
        });

    // ── getCurrentState() ─────────────────────────────────────────────────────
    opts = opts.withNativeFunction ("getCurrentState",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            complete (buildTuningStateJson());
        });

    // ── setStartMidi(value) ─────────────────────────────────────────────────
    opts = opts.withNativeFunction ("setStartMidi",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 1)
                processor.setStartMidi ((double) args[0]);
            complete (juce::var());
        });

    // ── checkForUpdates() ─────────────────────────────────────────────────────
    opts = opts.withNativeFunction ("checkForUpdates",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            processor.checkForDataUpdates (
                [complete] (std::vector<juce::String> updated)
                {
                    juce::Array<juce::var> arr;
                    for (const auto& s : updated) arr.add (juce::var (s));
                    complete (juce::var (arr));
                },
                [complete] () { complete (juce::var (juce::Array<juce::var>())); },
                [complete] (juce::String /*err*/) { complete (juce::var (juce::Array<juce::var>())); });
        });

    // ── getMaqamMidiDragData() ────────────────────────────────────────────────
    // Returns MIDI file data (base64) + filename for drag-and-drop export.
    // Only returns data if a maqam is currently selected.
    opts = opts.withNativeFunction ("getMaqamMidiDragData",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            complete (buildMaqamMidiDragData());
        });

    // ── saveMaqamMidiFile() ──────────────────────────────────────────────────
    // Saves MIDI file to Downloads folder and returns the path.
    opts = opts.withNativeFunction ("saveMaqamMidiFile",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            complete (saveMaqamMidiFile());
        });

    // ── beginSliderGesture(chromaticIndex) ───────────────────────────────────
    // Called on mousedown for DAW automation gesture marking.
    opts = opts.withNativeFunction ("beginSliderGesture",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 1)
                processor.beginSliderGesture ((int) args[0]);
            complete (juce::var());
        });

    // ── endSliderGesture(chromaticIndex) ─────────────────────────────────────
    // Called on mouseup for DAW automation gesture marking.
    opts = opts.withNativeFunction ("endSliderGesture",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 1)
                processor.endSliderGesture ((int) args[0]);
            complete (juce::var());
        });

    // ── beginPresetGesture() ─────────────────────────────────────────────────
    // Called before preset selection for DAW automation gesture marking.
    opts = opts.withNativeFunction ("beginPresetGesture",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            processor.beginPresetGesture();
            complete (juce::var());
        });

    // ── endPresetGesture() ───────────────────────────────────────────────────
    // Called after preset selection for DAW automation gesture marking.
    opts = opts.withNativeFunction ("endPresetGesture",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            processor.endPresetGesture();
            complete (juce::var());
        });

    // ── setReferenceFreqCents(cents) ───────────────────────────────────────
    // Fire-and-forget during continuous knob drag — updates MTS-ESP immediately.
    opts = opts.withNativeFunction ("setReferenceFreqCents",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 1)
                processor.setReferenceCentsOffset ((double) args[0]);
            complete (juce::var());
        });

    // ── setReferenceFreqCentsFinalize(cents) ────────────────────────────────
    // Called on mouseup — updates tuning + returns full state for WebView sync.
    opts = opts.withNativeFunction ("setReferenceFreqCentsFinalize",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 1)
                processor.finalizeReferenceCentsOffset ((double) args[0]);
            complete (buildTuningStateJson());
        });

    // ── beginRefFreqGesture() ───────────────────────────────────────────────
    opts = opts.withNativeFunction ("beginRefFreqGesture",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            processor.beginRefFreqGesture();
            complete (juce::var());
        });

    // ── endRefFreqGesture() ─────────────────────────────────────────────────
    opts = opts.withNativeFunction ("endRefFreqGesture",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            processor.endRefFreqGesture();
            complete (juce::var());
        });

    // ── setOscillatorEnabled(enabled) ──────────────────────────────────────
    // Toggle internal reference oscillator on/off. Fire-and-forget.
    opts = opts.withNativeFunction ("setOscillatorEnabled",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 1)
                processor.setOscillatorEnabled ((bool) args[0]);
            complete (juce::var());
        });

    // ── setHeptEnabled(enabled) ─────────────────────────────────────────
    // Toggle heptatonic keyboard mapping on/off. Fire-and-forget.
    opts = opts.withNativeFunction ("setHeptEnabled",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 1)
                processor.setHeptEnabled ((bool) args[0]);
            complete (juce::var());
        });

    // ── startMidiLearn(presetIndex) ─────────────────────────────────────────
    // Start MIDI Learn for a preset. Next MIDI note received will be mapped.
    opts = opts.withNativeFunction ("startMidiLearn",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            const int presetIdx = args.size() > 0 ? (int) args[0] : -1;
            processor.startMidiLearn (presetIdx);
            complete (juce::var (presetIdx));
        });

    // ── cancelMidiLearn() ────────────────────────────────────────────────────
    // Cancel MIDI Learn mode.
    opts = opts.withNativeFunction ("cancelMidiLearn",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            processor.cancelMidiLearn();
            complete (juce::var());
        });

    // ── getMidiLearnTarget() ─────────────────────────────────────────────────
    // Get the preset index currently in MIDI Learn mode (-1 if none).
    opts = opts.withNativeFunction ("getMidiLearnTarget",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            complete (juce::var (processor.getMidiLearnTarget()));
        });

    // ── getMidiPresetNote(presetIndex) ───────────────────────────────────────
    // Get the MIDI note mapped to a preset (-1 if unmapped).
    opts = opts.withNativeFunction ("getMidiPresetNote",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            const int presetIdx = args.size() > 0 ? (int) args[0] : -1;
            complete (juce::var (processor.getMidiPresetNote (presetIdx)));
        });

    // ── clearMidiPresetNote(presetIndex) ─────────────────────────────────────
    // Clear the MIDI note mapping for a preset.
    opts = opts.withNativeFunction ("clearMidiPresetNote",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            const int presetIdx = args.size() > 0 ? (int) args[0] : -1;
            processor.clearMidiPresetNote (presetIdx);
            complete (juce::var());
        });

    // ── clearAllMidiPresetNotes() ────────────────────────────────────────────
    // Clear all MIDI preset note mappings.
    opts = opts.withNativeFunction ("clearAllMidiPresetNotes",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            processor.clearAllMidiPresetNotes();
            complete (juce::var());
        });

    // ── setMidiPresetChannel(channel) ────────────────────────────────────────
    // Set the MIDI channel for preset triggering. 0 = any channel, 1-16 = specific.
    opts = opts.withNativeFunction ("setMidiPresetChannel",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            const int channel = args.size() > 0 ? (int) args[0] : 0;
            processor.setMidiPresetChannel (channel);
            complete (juce::var (channel));
        });

    // ── getMidiPresetChannel() ───────────────────────────────────────────────
    // Get the current MIDI channel for preset triggering.
    opts = opts.withNativeFunction ("getMidiPresetChannel",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            complete (juce::var (processor.getMidiPresetChannel()));
        });

    // ── saveStateFile() ─────────────────────────────────────────────────────
    // Opens a native Save dialog and writes the current state as a .tanghim JSON file.
    opts = opts.withNativeFunction ("saveStateFile",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            auto json = processor.buildStateJson();
            fileChooser = std::make_unique<juce::FileChooser> (
                "Save Tanghim State",
                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                    .getChildFile ("untitled.tanghim"),
                "*.tanghim");
            fileChooser->launchAsync (
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [complete, json] (const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file != juce::File())
                    {
                        // Ensure .tanghim extension
                        if (! file.hasFileExtension ("tanghim"))
                            file = file.withFileExtension ("tanghim");
                        file.replaceWithText (json);
                        complete (juce::var (file.getFileName()));
                    }
                    else
                    {
                        complete (juce::var());  // cancelled
                    }
                });
        });

    // ── loadStateFile() ───────────────────────────────────────────────────────
    // Opens a native Load dialog and restores state from a .tanghim JSON file.
    opts = opts.withNativeFunction ("loadStateFile",
        [this] (const juce::Array<juce::var>& /*args*/, Completion complete)
        {
            fileChooser = std::make_unique<juce::FileChooser> (
                "Load Tanghim State",
                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                "*.tanghim");
            fileChooser->launchAsync (
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, complete] (const juce::FileChooser& fc)
                {
                    auto file = fc.getResult();
                    if (file != juce::File() && file.existsAsFile())
                    {
                        auto content = file.loadFileAsString();

                        // Check if file contains a maqam before restoring
                        // (JS needs this to handle the two-event sequence from loadTuningSystem)
                        auto parsed = juce::JSON::parse (content);
                        bool hasMaqam = false;
                        if (auto* root = parsed.getDynamicObject())
                            hasMaqam = root->getProperty ("maqam").getDynamicObject() != nullptr;

                        processor.restoreStateFromJson (content);

                        auto* result = new juce::DynamicObject();
                        result->setProperty ("filename", file.getFileName());
                        result->setProperty ("hasMaqam", hasMaqam);
                        complete (juce::var (result));
                    }
                    else
                    {
                        complete (juce::var());  // cancelled
                    }
                });
        });

    return opts;
}

// ── JSON builders ─────────────────────────────────────────────────────────────

juce::var NativeBridge::buildTuningStateJson() const
{
    const auto& state = processor.getActiveTuningState();
    auto* root = new juce::DynamicObject();

    root->setProperty ("systemId",     processor.getCurrentSystemId());
    root->setProperty ("startingNote", processor.getCurrentStartingNote());
    root->setProperty ("isMtsTransmitter", processor.isMtsTransmitter());
    root->setProperty ("oscillatorEnabled", processor.getOscillatorEnabled());
    root->setProperty ("heptEnabled", processor.getHeptEnabled());

    const int totalReceivers = processor.mtsNumReceivers();
    const auto receiverCounts = processor.getReceiverCounts();
    const int tanghimTotal = receiverCounts.mpeReceivers + receiverCounts.monoPbReceivers;
    const int mtsNative    = std::max (0, totalReceivers - tanghimTotal);

    root->setProperty ("mtsReceivers",    totalReceivers);
    root->setProperty ("mtsNativeCount",  mtsNative);
    root->setProperty ("mpeCount",        receiverCounts.mpeReceivers);
    root->setProperty ("monoPbCount",     receiverCounts.monoPbReceivers);
    root->setProperty ("pluginVersion",    juce::String (PLUGIN_VERSION));
    root->setProperty ("buildTimestamp",   juce::String (BUILD_TIMESTAMP));

    // Reference frequency data
    root->setProperty ("referenceFreqCents",  processor.getReferenceCentsOffset());
    root->setProperty ("referenceFreqHz",     processor.getReferenceCurrentHz());
    root->setProperty ("referenceDefaultHz",  processor.getReferenceDefaultHz());
    root->setProperty ("referenceNoteName",   processor.getReferenceNoteDisplayName());

    // 12 slider slots
    juce::Array<juce::var> slots;
    for (int i = 0; i < 12; ++i)
    {
        const auto& slot = state.slots[(size_t) i];
        auto* s = new juce::DynamicObject();
        s->setProperty ("ipnRef",        slot.ipnReference);
        s->setProperty ("selectedIndex", slot.selectedIndex);
        s->setProperty ("isLocked",      slot.isLocked());
        s->setProperty ("centsOffset",   slot.centsOffset);

        juce::Array<juce::var> variants;
        for (const auto& v : slot.variants)
            variants.add (pitchClassToVar (v));
        s->setProperty ("variants", variants);

        slots.add (juce::var (s));
    }
    root->setProperty ("slots", slots);

    // Presets
    root->setProperty ("presets", buildPresetsJson());

    // Per-note variant overrides (sparse: only entries where override >= 0)
    auto* perNoteObj = new juce::DynamicObject();
    for (int i = 0; i < 128; ++i)
    {
        if (state.perNoteVariantOverrides[(size_t) i] >= 0)
            perNoteObj->setProperty (juce::String (i), state.perNoteVariantOverrides[(size_t) i]);
    }
    root->setProperty ("perNoteOverrides", juce::var (perNoteObj));

    // Note names map: chromaticIndex → { octave → PAO display name }
    // Uses effectiveVariantIndex() to respect per-note overrides.
    auto* noteNamesObj = new juce::DynamicObject();
    {
        const auto& allPCs = processor.getCurrentPitchClasses();

        for (int ci = 0; ci < 12; ++ci)
        {
            std::map<int, std::vector<const PitchClass*>> byOctave;
            for (const auto& pc : allPCs)
            {
                if (chromaticIndexForIpnRef (pc.ipnReference) == ci
                    && pc.midiNoteNumber >= 0 && pc.midiNoteNumber < 128)
                {
                    const int ipnOct = (pc.midiNoteNumber / 12) - 1;
                    byOctave[ipnOct].push_back (&pc);
                }
            }

            auto* octaveMap = new juce::DynamicObject();
            for (auto& [oct, pcs] : byOctave)
            {
                std::sort (pcs.begin(), pcs.end(),
                           [] (const PitchClass* a, const PitchClass* b)
                           { return a->cents < b->cents; });

                // Use the per-note-aware effective index for this MIDI note
                const int midiNote = (oct + 1) * 12 + ci;
                const int effectiveIdx = (midiNote >= 0 && midiNote < 128)
                    ? state.effectiveVariantIndex (midiNote)
                    : state.slots[(size_t) ci].selectedIndex;

                const int idx = juce::jlimit (0, (int) pcs.size() - 1, effectiveIdx);
                octaveMap->setProperty (juce::String (oct), pcs[(size_t) idx]->noteNameDisplay);
            }

            noteNamesObj->setProperty (juce::String (ci), juce::var (octaveMap));
        }
    }
    root->setProperty ("noteNames", juce::var (noteNamesObj));

    // paoNameMap: maps every PAO idName → chromaticIndex (from ALL pitch classes, all octaves).
    // This is needed by the JS side for degree highlighting and transposition sorting,
    // since slot variants only cover MIDI 48-59 and miss register-specific names.
    auto* paoNameMapObj = new juce::DynamicObject();
    {
        const auto& allPCs = processor.getCurrentPitchClasses();
        for (const auto& pc : allPCs)
        {
            if (pc.noteName.isEmpty()) continue;
            const int ci = chromaticIndexForIpnRef (pc.ipnReference);
            if (ci >= 0)
                paoNameMapObj->setProperty (pc.noteName, ci);
        }
    }
    root->setProperty ("paoNameMap", juce::var (paoNameMapObj));

    // paoOrder: unique PAO idNames sorted by (octave, pitchClassIndex) ascending.
    // This gives correct pitch order across all octaves — qarār (lower octave) before
    // base, and jawāb (higher octave) after.
    //
    // paoNameInfo: PAO idName → { englishName, solfege } for display in transposition dropdown.
    juce::Array<juce::var> paoOrderArr;
    auto* paoNameInfoObj = new juce::DynamicObject();
    {
        struct PaoEntry { juce::String name; juce::String englishName; juce::String solfege; };
        std::map<std::pair<int, int>, PaoEntry> sortedEntries;
        juce::StringArray seen;
        const auto& allPCs = processor.getCurrentPitchClasses();
        for (const auto& pc : allPCs)
        {
            if (pc.noteName.isEmpty()) continue;
            if (! seen.contains (pc.noteName))
            {
                seen.add (pc.noteName);
                sortedEntries[{ pc.octave, pc.pitchClassIndex }] =
                    { pc.noteName, pc.englishName, pc.solfege };
            }
        }
        // std::map iterates in ascending key order: (0,0), (0,1), ..., (1,0), (1,1), ...
        for (const auto& [key, entry] : sortedEntries)
        {
            paoOrderArr.add (juce::var (entry.name));

            auto* info = new juce::DynamicObject();
            info->setProperty ("englishName", entry.englishName);
            info->setProperty ("solfege",     entry.solfege);
            paoNameInfoObj->setProperty (entry.name, juce::var (info));
        }
    }
    root->setProperty ("paoOrder", juce::var (paoOrderArr));
    root->setProperty ("paoNameInfo", juce::var (paoNameInfoObj));

    // Degree-aware IPN + solfege references (from maqam detail API — e.g. Saba shows "Gb" not "F#")
    auto* degreeIpnMapObj = new juce::DynamicObject();
    auto* degreeSolfegeMapObj = new juce::DynamicObject();
    {
        const auto& ipnRefs = processor.getDegreeIpnRefs();
        const auto& solfegeRefs = processor.getDegreeSolfegeRefs();
        for (int i = 0; i < 12; ++i)
        {
            if (ipnRefs[(size_t) i].isNotEmpty())
                degreeIpnMapObj->setProperty (juce::String (i), ipnRefs[(size_t) i]);
            if (solfegeRefs[(size_t) i].isNotEmpty())
                degreeSolfegeMapObj->setProperty (juce::String (i), solfegeRefs[(size_t) i]);
        }
    }
    root->setProperty ("degreeIpnMap", juce::var (degreeIpnMapObj));
    root->setProperty ("degreeSolfegeMap", juce::var (degreeSolfegeMapObj));

    // Maqam selection state (for session recall + JS sync)
    root->setProperty ("selectedMaqamId",        processor.getCurrentMaqamId());
    root->setProperty ("transpositionIndex",     processor.getCurrentTranspositionIdx());
    root->setProperty ("activePresetIndex",      processor.getCurrentActivePresetIdx());
    root->setProperty ("startMidi",              processor.getCurrentStartMidi());
    root->setProperty ("sessionRecallInProgress", processor.getSessionRecallInProgress());
    root->setProperty ("hasRecalledSessionState", processor.getHasRecalledSessionState());
    root->setProperty ("midiPresetChannel", processor.getMidiPresetChannel());
    root->setProperty ("midiLearnTarget", processor.getMidiLearnTarget());

    // Per-preset MIDI note mappings (array of 16 ints, -1 = unmapped)
    juce::Array<juce::var> midiNotesArr;
    for (int i = 0; i < 16; ++i)
        midiNotesArr.add (juce::var (processor.getMidiPresetNote (i)));
    root->setProperty ("midiPresetNotes", midiNotesArr);

    juce::Array<juce::var> degArr;
    for (const auto& name : processor.getCurrentDegreeNames())
        degArr.add (juce::var (name));
    root->setProperty ("degreeNames", degArr);

    return juce::var (root);
}

juce::var NativeBridge::buildTuningSystemsJson() const
{
    const auto& systems = processor.getTuningSystems();
    juce::Array<juce::var> arr;

    for (const auto& ts : systems)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id",          ts.id);
        obj->setProperty ("displayName", ts.displayName);
        obj->setProperty ("shortName",   ts.shortName);
        obj->setProperty ("year",        ts.year);
        obj->setProperty ("yearStr",     ts.yearStr);

        juce::Array<juce::var> notes;
        for (int i = 0; i < ts.startingNoteIds.size(); ++i)
        {
            auto* n = new juce::DynamicObject();
            n->setProperty ("id",          ts.startingNoteIds[i]);
            n->setProperty ("displayName", ts.startingNoteDisplayNames[i]);
            notes.add (juce::var (n));
        }
        obj->setProperty ("startingNotes", notes);
        arr.add (juce::var (obj));
    }

    return juce::var (arr);
}

juce::var NativeBridge::buildPresetsJson() const
{
    const auto& presets = processor.getPresets();
    juce::Array<juce::var> arr;

    for (const auto& p : presets)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("isAssigned",    p.isAssigned);
        obj->setProperty ("maqamId",       p.maqamIdName);
        obj->setProperty ("maqamDisplay",  p.maqamDisplayName);
        obj->setProperty ("baseMaqamId",   p.baseMaqamIdName);
        obj->setProperty ("isTransposed",  p.isTransposed);
        obj->setProperty ("tonicNote",     p.tonicNoteName);
        obj->setProperty ("tonicIpn",      p.tonicIpnRef);
        obj->setProperty ("tonicSolfege",  p.tonicSolfege);
        obj->setProperty ("setIndex",      p.pitchClassSetIndex);

        juce::Array<juce::var> sp;
        for (int pos : p.sliderPositions) sp.add (juce::var (pos));
        obj->setProperty ("sliderPositions", sp);

        juce::Array<juce::var> co;
        for (double cents : p.centsOffsets) co.add (juce::var (cents));
        obj->setProperty ("centsOffsets", co);

        juce::Array<juce::var> dn;
        for (const auto& name : p.degreeNames) dn.add (juce::var (name));
        obj->setProperty ("degreeNames", dn);

        obj->setProperty ("tuningSystemId", p.tuningSystemId);
        obj->setProperty ("startingNote",   p.startingNote);

        arr.add (juce::var (obj));
    }

    return juce::var (arr);
}

juce::var NativeBridge::buildMaqamListJson() const
{
    const auto& list = processor.getMaqamList();
    juce::Array<juce::var> arr;

    auto degreesToVar = [] (const MaqamDegrees& deg) -> juce::var
    {
        auto* obj = new juce::DynamicObject();
        juce::Array<juce::var> asc, desc;
        for (const auto& s : deg.ascending)  asc.add (juce::var (s));
        for (const auto& s : deg.descending) desc.add (juce::var (s));
        obj->setProperty ("ascending",  asc);
        obj->setProperty ("descending", desc);
        return juce::var (obj);
    };

    for (const auto& mle : list)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("maqamId",       mle.maqamId);
        obj->setProperty ("maqamDisplay",  mle.maqamDisplay);
        obj->setProperty ("familyId",      mle.familyId);
        obj->setProperty ("familyDisplay", mle.familyDisplay);
        obj->setProperty ("tonicId",       mle.tonicId);
        obj->setProperty ("tonicDisplay",  mle.tonicDisplay);
        obj->setProperty ("degrees",       degreesToVar (mle.degrees));

        juce::Array<juce::var> transArr;
        for (const auto& t : mle.transpositions)
        {
            auto* tObj = new juce::DynamicObject();
            tObj->setProperty ("tonicId",      t.tonicId);
            tObj->setProperty ("tonicDisplay", t.tonicDisplay);
            tObj->setProperty ("degrees",      degreesToVar (t.degrees));
            transArr.add (juce::var (tObj));
        }
        obj->setProperty ("transpositions", transArr);

        arr.add (juce::var (obj));
    }

    return juce::var (arr);
}

juce::var NativeBridge::pitchClassToVar (const PitchClass& pc) const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("noteName",           pc.noteName);
    obj->setProperty ("noteNameDisplay",    pc.noteNameDisplay);
    obj->setProperty ("englishName",        pc.englishName);
    obj->setProperty ("solfege",            pc.solfege);
    obj->setProperty ("midiNoteNumber",     pc.midiNoteNumber);
    obj->setProperty ("midiCentsDeviation", pc.midiCentsDeviation);
    obj->setProperty ("cents",              pc.cents);
    obj->setProperty ("fraction",           pc.fraction);
    obj->setProperty ("ipnReference",       pc.ipnReference);
    return juce::var (obj);
}

juce::var NativeBridge::buildMaqamMidiDragData() const
{
    // Check if a maqam is selected
    const juce::String maqamId = processor.getCurrentMaqamId();
    if (maqamId.isEmpty())
        return juce::var(); // null

    const auto& degreeNames = processor.getCurrentDegreeNames();
    if (degreeNames.empty())
        return juce::var();

    // Build paoNameMap: PAO idName → chromaticIndex (from ALL pitch classes)
    std::map<juce::String, int> paoNameMap;
    const auto& allPCs = processor.getCurrentPitchClasses();
    for (const auto& pc : allPCs)
    {
        if (pc.noteName.isEmpty()) continue;
        const int ci = chromaticIndexForIpnRef (pc.ipnReference);
        if (ci >= 0)
            paoNameMap[pc.noteName] = ci;
    }

    // Find tonic MIDI note (first degree)
    const juce::String tonicName = degreeNames[0];
    auto tonicIt = paoNameMap.find (tonicName);
    if (tonicIt == paoNameMap.end())
        return juce::var();
    const int tonicChromaticIdx = tonicIt->second;

    // Find tonic MIDI from pitch classes (prefer octave 3 / MIDI 48-59 range)
    int tonicMidi = -1;
    for (const auto& pc : allPCs)
    {
        if (pc.noteName == tonicName && pc.midiNoteNumber >= 48 && pc.midiNoteNumber < 60)
        {
            tonicMidi = pc.midiNoteNumber;
            break;
        }
    }
    // Fallback: use any MIDI note with this name
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
    if (tonicMidi < 0)
        return juce::var();

    // Build list of MIDI notes for all degrees
    std::vector<int> midiNotes;
    for (const auto& degreeName : degreeNames)
    {
        auto it = paoNameMap.find (degreeName);
        if (it == paoNameMap.end()) continue;
        const int degreeChromatic = it->second;

        // Calculate MIDI note relative to tonic
        int interval = degreeChromatic - tonicChromaticIdx;
        if (interval < 0) interval += 12; // wrap around octave

        int midiNote = tonicMidi + interval;
        // If interval wraps past 12, the note is in the next octave
        // (handled by the interval calculation)

        if (midiNote >= 0 && midiNote <= 127)
            midiNotes.push_back (midiNote);
    }

    if (midiNotes.empty())
        return juce::var();

    // Get maqam display info
    MidiFileGenerator::MaqamInfo info;
    info.maqamDisplay    = processor.getCurrentMaqamDisplay();
    info.tonicPaoDisplay = processor.getCurrentTonicDisplay();
    info.tonicIpn        = processor.getCurrentTonicEnglish();
    info.tonicSolfege    = processor.getCurrentTonicSolfege();
    info.midiNotes       = midiNotes;

    // Fallback display names from pitch class data if not set
    if (info.maqamDisplay.isEmpty())
        info.maqamDisplay = maqamId;
    if (info.tonicPaoDisplay.isEmpty())
        info.tonicPaoDisplay = tonicName;
    if (info.tonicIpn.isEmpty())
    {
        // Build IPN from tonic MIDI note
        static const char* IPN_NAMES[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int octave = (tonicMidi / 12) - 1;
        info.tonicIpn = juce::String (IPN_NAMES[tonicMidi % 12]) + juce::String (octave);
    }
    if (info.tonicSolfege.isEmpty())
    {
        // Try to get solfege from pitch class data
        for (const auto& pc : allPCs)
        {
            if (pc.midiNoteNumber == tonicMidi)
            {
                info.tonicSolfege = pc.solfege;
                break;
            }
        }
    }

    // Generate MIDI file
    auto midiData = MidiFileGenerator::generate (info);
    auto filename = MidiFileGenerator::buildFilename (info);

    DBG ("MIDI data size: " << (int) midiData.size() << " bytes");
    DBG ("MIDI filename: " << filename);

    // Encode as base64 (remove whitespace for data URI compatibility)
    juce::MemoryBlock block (midiData.data(), midiData.size());
    juce::String base64 = juce::Base64::toBase64 (block.getData(), block.getSize());

    DBG ("Base64 length: " << base64.length());
    DBG ("Base64 first 50: " << base64.substring (0, 50));

    auto* result = new juce::DynamicObject();
    result->setProperty ("filename", filename);
    result->setProperty ("midiBase64", base64);
    result->setProperty ("maqamDisplay", info.maqamDisplay);
    result->setProperty ("tonicDisplay", info.tonicPaoDisplay);
    return juce::var (result);
}

juce::var NativeBridge::saveMaqamMidiFile() const
{
    // Check if a maqam is selected
    const juce::String maqamId = processor.getCurrentMaqamId();
    if (maqamId.isEmpty())
        return juce::var();

    const auto& degreeNames = processor.getCurrentDegreeNames();
    if (degreeNames.empty())
        return juce::var();

    // Build paoNameMap: PAO idName → chromaticIndex
    std::map<juce::String, int> paoNameMap;
    const auto& allPCs = processor.getCurrentPitchClasses();
    for (const auto& pc : allPCs)
    {
        if (pc.noteName.isEmpty()) continue;
        const int ci = chromaticIndexForIpnRef (pc.ipnReference);
        if (ci >= 0)
            paoNameMap[pc.noteName] = ci;
    }

    // Find tonic
    const juce::String tonicName = degreeNames[0];
    auto tonicIt = paoNameMap.find (tonicName);
    if (tonicIt == paoNameMap.end())
        return juce::var();
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
    if (tonicMidi < 0)
        return juce::var();

    // Build MIDI notes list
    std::vector<int> midiNotes;
    for (const auto& degreeName : degreeNames)
    {
        auto it = paoNameMap.find (degreeName);
        if (it == paoNameMap.end()) continue;
        const int degreeChromatic = it->second;
        int interval = degreeChromatic - tonicChromaticIdx;
        if (interval < 0) interval += 12;
        int midiNote = tonicMidi + interval;
        if (midiNote >= 0 && midiNote <= 127)
            midiNotes.push_back (midiNote);
    }

    if (midiNotes.empty())
        return juce::var();

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
        const int octave = (tonicMidi / 12) - 1;
        info.tonicIpn = juce::String (IPN_NAMES[tonicMidi % 12]) + juce::String (octave);
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

    // Generate MIDI file
    auto midiData = MidiFileGenerator::generate (info);
    auto filename = MidiFileGenerator::buildFilename (info);

    // Save to Downloads folder
    auto downloadsDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                            .getChildFile ("Downloads");
    auto outputFile = downloadsDir.getChildFile (filename);

    // Make filename unique if it already exists
    int counter = 1;
    while (outputFile.existsAsFile())
    {
        auto stem = filename.upToLastOccurrenceOf (".", false, false);
        auto ext = filename.fromLastOccurrenceOf (".", true, false);
        outputFile = downloadsDir.getChildFile (stem + "_" + juce::String (counter++) + ext);
    }

    // Write the file
    if (! outputFile.replaceWithData (midiData.data(), midiData.size()))
        return juce::var();

    auto* result = new juce::DynamicObject();
    result->setProperty ("path", outputFile.getFullPathName());
    result->setProperty ("filename", outputFile.getFileName());
    return juce::var (result);
}

