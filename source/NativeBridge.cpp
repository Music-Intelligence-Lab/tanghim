#include "NativeBridge.h"

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

    // ── applyPreset(presetIndex) ──────────────────────────────────────────────
    opts = opts.withNativeFunction ("applyPreset",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            if (args.size() >= 1)
                processor.applyPreset ((int) args[0]);
            complete (buildTuningStateJson());
        });

    // ── assignPreset(index, maqamId, maqamDisplay, baseMaqamId, isTransposed,
    //                 tonicNote, tonicIpn, setIndex, [sliderPos x12]) ────────
    opts = opts.withNativeFunction ("assignPreset",
        [this] (const juce::Array<juce::var>& args, Completion complete)
        {
            const int idx               = args.size() > 0 ? (int) args[0] : 0;
            const juce::String id       = args.size() > 1 ? args[1].toString() : "";
            const juce::String disp     = args.size() > 2 ? args[2].toString() : "";
            const juce::String baseId   = args.size() > 3 ? args[3].toString() : "";
            const bool transposed       = args.size() > 4 ? (bool) args[4] : false;
            const juce::String tonicN   = args.size() > 5 ? args[5].toString() : "";
            const juce::String tonicIpn = args.size() > 6 ? args[6].toString() : "";
            const int setIdx            = args.size() > 7 ? (int) args[7] : -1;

            std::array<int, 12> positions;
            positions.fill (0);
            if (args.size() > 8 && args[8].isArray())
                for (int i = 0; i < 12 && i < args[8].size(); ++i)
                    positions[(size_t) i] = (int) args[8][i];

            std::vector<juce::String> degreeNames;
            if (args.size() > 9 && args[9].isArray())
                for (int i = 0; i < args[9].size(); ++i)
                    degreeNames.push_back (args[9][i].toString());

            processor.assignPreset (idx, id, disp, baseId, transposed, tonicN, tonicIpn, setIdx, positions, degreeNames);
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

    const int totalReceivers = processor.mtsNumReceivers();
    const auto receiverCounts = processor.getReceiverCounts();
    const int tanghimTotal = receiverCounts.mpeReceivers + receiverCounts.monoPbReceivers;
    const int mtsNative    = std::max (0, totalReceivers - tanghimTotal);

    root->setProperty ("mtsReceivers",    totalReceivers);
    root->setProperty ("mtsNativeCount",  mtsNative);
    root->setProperty ("mpeCount",        receiverCounts.mpeReceivers);
    root->setProperty ("monoPbCount",     receiverCounts.monoPbReceivers);
    root->setProperty ("pluginVersion",    juce::String (PLUGIN_VERSION) + " (" + __DATE__ + " " + __TIME__ + ")");

    // 12 slider slots
    juce::Array<juce::var> slots;
    for (int i = 0; i < 12; ++i)
    {
        const auto& slot = state.slots[(size_t) i];
        auto* s = new juce::DynamicObject();
        s->setProperty ("ipnRef",        slot.ipnReference);
        s->setProperty ("selectedIndex", slot.selectedIndex);
        s->setProperty ("isLocked",      slot.isLocked());

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
        obj->setProperty ("setIndex",      p.pitchClassSetIndex);

        juce::Array<juce::var> sp;
        for (int pos : p.sliderPositions) sp.add (juce::var (pos));
        obj->setProperty ("sliderPositions", sp);

        juce::Array<juce::var> dn;
        for (const auto& name : p.degreeNames) dn.add (juce::var (name));
        obj->setProperty ("degreeNames", dn);

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
    obj->setProperty ("midiNoteNumber",     pc.midiNoteNumber);
    obj->setProperty ("midiCentsDeviation", pc.midiCentsDeviation);
    obj->setProperty ("cents",              pc.cents);
    obj->setProperty ("fraction",           pc.fraction);
    obj->setProperty ("ipnReference",       pc.ipnReference);
    return juce::var (obj);
}

