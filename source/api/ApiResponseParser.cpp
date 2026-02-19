#include "ApiResponseParser.h"
#include <algorithm>

// ── Public API ────────────────────────────────────────────────────────────────

std::vector<TuningSystem> ApiResponseParser::parseTuningSystemsList (const juce::var& json)
{
    std::vector<TuningSystem> result;

    // Response: { count: N, data: [ { tuningSystem: {...}, startingNotes: {...}, stats: {...} }, ... ] }
    const juce::var* arr = nullptr;
    if (auto* root = json.getDynamicObject())
    {
        const juce::var* data = root->getProperties().getVarPointer ("data");
        if (data && data->isArray()) arr = data;
    }
    if (arr == nullptr && json.isArray()) arr = &json;
    if (arr == nullptr) return result;

    for (int i = 0; i < arr->size(); ++i)
        result.push_back (parseSingleTuningSystem ((*arr)[i]));

    return result;
}

std::vector<PitchClass> ApiResponseParser::parsePitchClasses (const juce::var& json)
{
    std::vector<PitchClass> result;

    // Response: { tuningSystem: {...}, pitchClasses: [...], ... }
    const juce::var* arr = nullptr;
    if (auto* root = json.getDynamicObject())
    {
        const juce::var* pcs = root->getProperties().getVarPointer ("pitchClasses");
        if (pcs && pcs->isArray()) arr = pcs;
        else
        {
            const juce::var* data = root->getProperties().getVarPointer ("data");
            if (data && data->isArray()) arr = data;
        }
    }
    if (arr == nullptr && json.isArray()) arr = &json;
    if (arr == nullptr) return result;

    for (int i = 0; i < arr->size(); ++i)
    {
        auto pc = parseSinglePitchClass ((*arr)[i]);
        if (pc.isValid())
            result.push_back (std::move (pc));
    }

    return result;
}

std::vector<TwelvePitchClassSet> ApiResponseParser::parseTwelvePitchClassSets (const juce::var& json)
{
    std::vector<TwelvePitchClassSet> result;

    // Response: { statistics: {...}, sets: [...] }
    const juce::var* arr = nullptr;
    if (auto* root = json.getDynamicObject())
    {
        const juce::var* sets = root->getProperties().getVarPointer ("sets");
        if (sets && sets->isArray()) arr = sets;
        else
        {
            const juce::var* data = root->getProperties().getVarPointer ("data");
            if (data && data->isArray()) arr = data;
        }
    }
    if (arr == nullptr && json.isArray()) arr = &json;
    if (arr == nullptr) return result;

    for (int i = 0; i < arr->size(); ++i)
        result.push_back (parseSingleTwelvePitchClassSet ((*arr)[i]));

    return result;
}

std::vector<MaqamListEntry> ApiResponseParser::parseMaqamList (const juce::var& json)
{
    std::vector<MaqamListEntry> result;

    // Response: { tuningSystem: {...}, ..., data: [ { maqam: {idName, displayName}, family: {idName, displayName}, tonic: {idName, displayName}, ... } ] }
    const juce::var* arr = nullptr;
    if (auto* root = json.getDynamicObject())
    {
        const juce::var* data = root->getProperties().getVarPointer ("data");
        if (data && data->isArray()) arr = data;
    }
    if (arr == nullptr && json.isArray()) arr = &json;
    if (arr == nullptr) return result;

    for (int i = 0; i < arr->size(); ++i)
    {
        const auto& entry = (*arr)[i];
        if (entry.getDynamicObject() == nullptr) continue;
        const auto& props = entry.getDynamicObject()->getProperties();

        MaqamListEntry mle;

        // maqam: { id, idName, displayName, version }
        const juce::var* maqamObj = props.getVarPointer ("maqam");
        if (maqamObj && maqamObj->getDynamicObject())
        {
            const auto& mp = maqamObj->getDynamicObject()->getProperties();
            const juce::var* id  = mp.getVarPointer ("idName");
            const juce::var* dn  = mp.getVarPointer ("displayName");
            if (id) mle.maqamId      = id->toString();
            if (dn) mle.maqamDisplay = dn->toString();
        }

        // family: { idName, displayName }
        const juce::var* familyObj = props.getVarPointer ("family");
        if (familyObj && familyObj->getDynamicObject())
        {
            const auto& fp = familyObj->getDynamicObject()->getProperties();
            const juce::var* id  = fp.getVarPointer ("idName");
            const juce::var* dn  = fp.getVarPointer ("displayName");
            if (id) mle.familyId      = id->toString();
            if (dn) mle.familyDisplay = dn->toString();
        }

        // tonic: { idName, displayName }
        const juce::var* tonicObj = props.getVarPointer ("tonic");
        if (tonicObj && tonicObj->getDynamicObject())
        {
            const auto& tp = tonicObj->getDynamicObject()->getProperties();
            const juce::var* id  = tp.getVarPointer ("idName");
            const juce::var* dn  = tp.getVarPointer ("displayName");
            if (id) mle.tonicId      = id->toString();
            if (dn) mle.tonicDisplay = dn->toString();
        }

        // maqamDegrees: { ascending: [...], descending: [...] }
        const juce::var* degreesObj = props.getVarPointer ("maqamDegrees");
        if (degreesObj && degreesObj->getDynamicObject())
            mle.degrees = parseMaqamDegrees (*degreesObj);

        // transpositions: [ { tonic: {idName, displayName}, maqamDegrees: {...} } ]
        const juce::var* transArr = props.getVarPointer ("transpositions");
        if (transArr && transArr->isArray())
        {
            for (int t = 0; t < transArr->size(); ++t)
            {
                const auto& tObj = (*transArr)[t];
                if (tObj.getDynamicObject() == nullptr) continue;
                const auto& tProps = tObj.getDynamicObject()->getProperties();

                MaqamTransposition mt;

                const juce::var* tTonic = tProps.getVarPointer ("tonic");
                if (tTonic && tTonic->getDynamicObject())
                {
                    const auto& ttProps = tTonic->getDynamicObject()->getProperties();
                    const juce::var* tid = ttProps.getVarPointer ("idName");
                    const juce::var* tdn = ttProps.getVarPointer ("displayName");
                    if (tid) mt.tonicId      = tid->toString();
                    if (tdn) mt.tonicDisplay = tdn->toString();
                }

                const juce::var* tDegrees = tProps.getVarPointer ("maqamDegrees");
                if (tDegrees && tDegrees->getDynamicObject())
                    mt.degrees = parseMaqamDegrees (*tDegrees);

                mle.transpositions.push_back (std::move (mt));
            }
        }

        if (mle.maqamId.isNotEmpty())
            result.push_back (std::move (mle));
    }

    return result;
}

std::vector<PitchClass> ApiResponseParser::parseMaqamDetail (const juce::var& json)
{
    std::vector<PitchClass> result;

    // Response: { maqam: {...}, pitchData: { ascending: [...], descending: [...] }, ... }
    const juce::var* arr = nullptr;
    if (auto* root = json.getDynamicObject())
    {
        const juce::var* pitchData = root->getProperties().getVarPointer ("pitchData");
        if (pitchData && pitchData->getDynamicObject())
        {
            const auto& pdProps = pitchData->getDynamicObject()->getProperties();
            const juce::var* ascending = pdProps.getVarPointer ("ascending");
            if (ascending && ascending->isArray())
                arr = ascending;
        }
    }
    if (arr == nullptr) return result;

    for (int i = 0; i < arr->size(); ++i)
    {
        auto pc = parseSinglePitchClass ((*arr)[i]);
        if (pc.isValid())
            result.push_back (std::move (pc));
    }

    return result;
}

bool ApiResponseParser::parseMidiNoteDeviation (const juce::String& s,
                                                int&    midiNoteOut,
                                                double& centsOut)
{
    const int spaceIdx = s.indexOfChar (' ');
    if (spaceIdx < 0) return false;
    midiNoteOut = s.substring (0, spaceIdx).getIntValue();
    centsOut    = s.substring (spaceIdx + 1).getDoubleValue();
    return true;
}

std::array<std::vector<PitchClass>, 12>
ApiResponseParser::buildVariantsPerSlot (const std::vector<PitchClass>& pitchClasses)
{
    std::array<std::vector<PitchClass>, 12> slots;

    // The 12 sliders represent MIDI notes C3 (48) through B3 (59).
    // We filter by MIDI note number rather than API octave because the API's
    // octave boundaries are tonic-relative (e.g. a G-based system has octave 1
    // spanning G2–F#3), while our sliders always show the C3–B3 register.
    for (const auto& pc : pitchClasses)
    {
        if (pc.midiNoteNumber < 48 || pc.midiNoteNumber > 59)
            continue;

        const int idx = chromaticIndexForIpnRef (pc.ipnReference);
        if (idx >= 0)
            slots[(size_t) idx].push_back (pc);
    }

    // Sort each slot's variants by ascending cents (lower pitch first)
    for (auto& slot : slots)
        std::sort (slot.begin(), slot.end(),
                   [] (const PitchClass& a, const PitchClass& b)
                   { return a.cents < b.cents; });

    return slots;
}

// ── Private helpers ───────────────────────────────────────────────────────────

PitchClass ApiResponseParser::parseSinglePitchClass (const juce::var& obj)
{
    PitchClass pc;
    if (obj.getDynamicObject() == nullptr) return pc;
    const auto& props = obj.getDynamicObject()->getProperties();

    auto get = [&] (const char* key) -> juce::var
    {
        const juce::var* v = props.getVarPointer (key);
        return v ? *v : juce::var();
    };

    pc.noteName         = get ("noteName").toString();
    pc.noteNameDisplay  = get ("noteNameDisplay").toString();
    if (pc.noteNameDisplay.isEmpty())
        pc.noteNameDisplay = pc.noteName;

    pc.englishName      = get ("englishName").toString();
    pc.abjadName        = get ("abjadName").toString();
    pc.fraction         = get ("fraction").toString();
    pc.cents            = get ("cents").toString().getDoubleValue();
    pc.frequency        = get ("frequency").toString().getDoubleValue();
    pc.pitchClassIndex  = (int) get ("pitchClassIndex");
    pc.octave           = (int) get ("octave");
    pc.version          = get ("version").toString();

    // Parse midiNotePlusCentsDeviation (API field name) or midiNoteDeviation (legacy)
    juce::String mnd = get ("midiNotePlusCentsDeviation").toString();
    if (mnd.isEmpty())
        mnd = get ("midiNoteDeviation").toString();
    if (mnd.isNotEmpty())
        parseMidiNoteDeviation (mnd, pc.midiNoteNumber, pc.midiCentsDeviation);

    // Use API-provided ipnReferenceNoteName if available, otherwise compute
    juce::String apiIpn = get ("ipnReferenceNoteName").toString();
    if (apiIpn.isNotEmpty())
        pc.ipnReference = apiIpn;
    else
        pc.ipnReference = PitchClass::ipnReferenceFromEnglishName (pc.englishName);

    return pc;
}

TuningSystem ApiResponseParser::parseSingleTuningSystem (const juce::var& obj)
{
    TuningSystem ts;
    if (obj.getDynamicObject() == nullptr) return ts;
    const auto& props = obj.getDynamicObject()->getProperties();

    auto get = [&] (const char* key) -> juce::var
    {
        const juce::var* v = props.getVarPointer (key);
        return v ? *v : juce::var();
    };

    // API wraps in { tuningSystem: {...}, startingNotes: {...}, stats: {...} }
    const juce::var* tsObj = props.getVarPointer ("tuningSystem");
    if (tsObj && tsObj->getDynamicObject())
    {
        const auto& tsProps = tsObj->getDynamicObject()->getProperties();
        auto tsGet = [&] (const char* key) -> juce::var
        {
            const juce::var* v = tsProps.getVarPointer (key);
            return v ? *v : juce::var();
        };

        ts.id          = tsGet ("idName").toString();
        if (ts.id.isEmpty()) ts.id = tsGet ("id").toString();
        ts.displayName = tsGet ("displayName").toString();
        ts.version     = tsGet ("version").toString();

        juce::String yearStr = tsGet ("year").toString();
        ts.year = yearStr.getIntValue();

        ts.pitchClassesPerOctave = (int) tsGet ("numberOfPitchClassesSingleOctave");
    }
    else
    {
        // Flat format fallback
        ts.id          = get ("idName").toString();
        if (ts.id.isEmpty()) ts.id = get ("id").toString();
        ts.displayName = get ("displayName").toString();
        ts.version     = get ("version").toString();
        ts.year        = get ("year").toString().getIntValue();
        ts.pitchClassesPerOctave = (int) get ("pitchClassesPerOctave");
    }

    if (ts.shortName.isEmpty()) ts.shortName = ts.displayName;
    if (ts.pitchClassesPerOctave <= 0) ts.pitchClassesPerOctave = 12;

    // Stats may have numberOfPitchClassesSingleOctave
    const juce::var* statsObj = props.getVarPointer ("stats");
    if (statsObj && statsObj->getDynamicObject())
    {
        const auto& statsProps = statsObj->getDynamicObject()->getProperties();
        const juce::var* npco = statsProps.getVarPointer ("numberOfPitchClassesSingleOctave");
        if (npco) ts.pitchClassesPerOctave = (int) *npco;

        const juce::var* refFreq = statsProps.getVarPointer ("referenceFrequency");
        if (refFreq) ts.referenceFrequency = refFreq->toString().getDoubleValue();
    }
    if (ts.referenceFrequency <= 0.0) ts.referenceFrequency = 440.0;

    // Parse starting notes: { idNames: [...], displayNames: [...] }
    const juce::var* snObj = props.getVarPointer ("startingNotes");
    if (snObj && snObj->getDynamicObject())
    {
        const auto& snProps = snObj->getDynamicObject()->getProperties();
        const juce::var* idNames = snProps.getVarPointer ("idNames");
        const juce::var* displayNames = snProps.getVarPointer ("displayNames");

        if (idNames && idNames->isArray())
        {
            for (int i = 0; i < idNames->size(); ++i)
                ts.startingNoteIds.add ((*idNames)[i].toString());
        }
        if (displayNames && displayNames->isArray())
        {
            for (int i = 0; i < displayNames->size(); ++i)
                ts.startingNoteDisplayNames.add ((*displayNames)[i].toString());
        }
    }
    else if (snObj && snObj->isArray())
    {
        // Legacy array-of-objects format: [{ id, displayName }, ...]
        for (int i = 0; i < snObj->size(); ++i)
        {
            const auto& entry = (*snObj)[i];
            if (entry.getDynamicObject() != nullptr)
            {
                const auto& ep = entry.getDynamicObject()->getProperties();
                const juce::var* sid = ep.getVarPointer ("id");
                const juce::var* sdn = ep.getVarPointer ("displayName");
                if (sid) ts.startingNoteIds.add (sid->toString());
                if (sdn) ts.startingNoteDisplayNames.add (sdn->toString());
                else if (sid) ts.startingNoteDisplayNames.add (sid->toString());
            }
        }
    }

    return ts;
}

TwelvePitchClassSet ApiResponseParser::parseSingleTwelvePitchClassSet (const juce::var& obj)
{
    TwelvePitchClassSet set;
    if (obj.getDynamicObject() == nullptr) return set;
    const auto& props = obj.getDynamicObject()->getProperties();

    auto get = [&] (const char* key) -> juce::var
    {
        const juce::var* v = props.getVarPointer (key);
        return v ? *v : juce::var();
    };

    // Source maqam: { sourceMaqam: { idName, displayName } }
    const juce::var* srcMaqam = props.getVarPointer ("sourceMaqam");
    if (srcMaqam && srcMaqam->getDynamicObject())
    {
        const auto& smProps = srcMaqam->getDynamicObject()->getProperties();
        const juce::var* smi = smProps.getVarPointer ("idName");
        const juce::var* smd = smProps.getVarPointer ("displayName");
        if (smi) set.sourceMaqamIdName      = smi->toString();
        if (smd) set.sourceMaqamDisplayName  = smd->toString();
    }
    else
    {
        set.sourceMaqamIdName      = get ("sourceMaqamIdName").toString();
        set.sourceMaqamDisplayName = get ("sourceMaqamDisplayName").toString();
    }

    // Parse pitchClassSet array (12 pitch class objects)
    const juce::var* slotsArr = props.getVarPointer ("pitchClassSet");
    if (slotsArr == nullptr) slotsArr = props.getVarPointer ("pitchClasses");
    if (slotsArr == nullptr) slotsArr = props.getVarPointer ("slots");
    if (slotsArr && slotsArr->isArray())
    {
        for (int i = 0; i < std::min (12, slotsArr->size()); ++i)
        {
            const auto& pcObj = (*slotsArr)[i];
            if (pcObj.getDynamicObject() != nullptr)
            {
                // 12-pitch-class-set items have limited fields:
                // { ipnReferenceNoteName, noteName, midiNoteDeviation }
                PitchClass pc;
                auto pcProps = pcObj.getDynamicObject()->getProperties();
                auto pcGet = [&] (const char* key) -> juce::var
                {
                    const juce::var* v = pcProps.getVarPointer (key);
                    return v ? *v : juce::var();
                };

                pc.noteName = pcGet ("noteName").toString();
                if (pc.noteName.isEmpty())
                    pc.noteName = pcGet ("noteNameDisplay").toString();
                pc.noteNameDisplay = pcGet ("noteNameDisplay").toString();
                if (pc.noteNameDisplay.isEmpty())
                    pc.noteNameDisplay = pc.noteName;

                pc.ipnReference = pcGet ("ipnReferenceNoteName").toString();

                juce::String mnd = pcGet ("midiNoteDeviation").toString();
                if (mnd.isNotEmpty())
                    parseMidiNoteDeviation (mnd, pc.midiNoteNumber, pc.midiCentsDeviation);

                pc.englishName = pcGet ("englishName").toString();

                set.slots[(size_t) i] = pc;
            }
        }
    }

    // Parse compatible maqamat list
    const juce::var* compat = props.getVarPointer ("compatibleMaqamat");
    if (compat == nullptr) compat = props.getVarPointer ("maqamat");
    if (compat && compat->isArray())
    {
        for (int i = 0; i < compat->size(); ++i)
            set.compatibleMaqamat.push_back (parseCompatibleMaqam ((*compat)[i]));
    }

    return set;
}

CompatibleMaqam ApiResponseParser::parseCompatibleMaqam (const juce::var& obj)
{
    CompatibleMaqam m;
    if (obj.getDynamicObject() == nullptr) return m;
    const auto& props = obj.getDynamicObject()->getProperties();

    auto get = [&] (const char* key) -> juce::var
    {
        const juce::var* v = props.getVarPointer (key);
        return v ? *v : juce::var();
    };

    m.maqamIdName      = get ("maqamIdName").toString();
    m.maqamDisplayName = get ("maqamDisplayName").toString();
    m.baseMaqamIdName  = get ("baseMaqamIdName").toString();
    m.isTransposed     = (bool) get ("isTransposed");
    m.version          = get ("version").toString();

    // Tonic info: { tonic: { ipnReferenceNoteName, noteNameIdName, noteNameDisplayName, positionInSet } }
    const juce::var* tonicObj = props.getVarPointer ("tonic");
    if (tonicObj && tonicObj->getDynamicObject())
    {
        const auto& tProps = tonicObj->getDynamicObject()->getProperties();
        const juce::var* tipn = tProps.getVarPointer ("ipnReferenceNoteName");
        const juce::var* tnn  = tProps.getVarPointer ("noteNameIdName");
        const juce::var* tnd  = tProps.getVarPointer ("noteNameDisplayName");
        if (tipn) m.tonicIpnRef   = tipn->toString();
        if (tnn)  m.tonicNoteName = tnn->toString();
        else if (tnd) m.tonicNoteName = tnd->toString();
    }
    else
    {
        // Flat format fallback
        m.tonicNoteName = get ("tonicNoteName").toString();
        m.tonicIpnRef   = get ("tonicIpnRef").toString();
    }

    return m;
}

MaqamDegrees ApiResponseParser::parseMaqamDegrees (const juce::var& obj)
{
    MaqamDegrees deg;
    if (obj.getDynamicObject() == nullptr) return deg;
    const auto& props = obj.getDynamicObject()->getProperties();

    auto parseStringArray = [] (const juce::var* arr) -> std::vector<juce::String>
    {
        std::vector<juce::String> result;
        if (arr && arr->isArray())
            for (int i = 0; i < arr->size(); ++i)
                result.push_back ((*arr)[i].toString());
        return result;
    };

    deg.ascending  = parseStringArray (props.getVarPointer ("ascending"));
    deg.descending = parseStringArray (props.getVarPointer ("descending"));
    return deg;
}
