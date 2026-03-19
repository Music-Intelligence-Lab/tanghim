#include "ApiDataCache.h"
#include <algorithm>

ApiDataCache::ApiDataCache()
{
    ensureCacheDirectory();
}

ApiDataCache::~ApiDataCache() = default;

// ── Tuning systems list ───────────────────────────────────────────────────────

void ApiDataCache::setTuningSystemsList (std::vector<TuningSystem> systems)
{
    // Sort chronologically by year
    std::sort (systems.begin(), systems.end(),
               [] (const TuningSystem& a, const TuningSystem& b)
               { return a.year != b.year ? a.year < b.year : a.yearStr < b.yearStr; });

    juce::ScopedLock sl (lock);
    tuningSystemsList = std::move (systems);
    hasSystems = true;
    saveSystemsListToDisk();
}

const std::vector<TuningSystem>& ApiDataCache::getTuningSystemsList() const
{
    juce::ScopedLock sl (lock);
    return tuningSystemsList;
}

bool ApiDataCache::hasTuningSystemsList() const
{
    juce::ScopedLock sl (lock);
    return hasSystems && ! tuningSystemsList.empty();
}

// ── Per-system data ───────────────────────────────────────────────────────────

juce::String ApiDataCache::makeKey (const juce::String& systemId,
                                    const juce::String& startingNote) const
{
    return systemId + ":" + startingNote;
}

bool ApiDataCache::hasData (const juce::String& systemId,
                             const juce::String& startingNote) const
{
    juce::ScopedLock sl (lock);
    const auto key = makeKey (systemId, startingNote);
    return cache.count (key) > 0 || lazyKeys.count (key) > 0;
}

bool ApiDataCache::hasMaqamList (const juce::String& systemId,
                                  const juce::String& startingNote) const
{
    juce::ScopedLock sl (lock);
    const auto key = makeKey (systemId, startingNote);

    // Ensure lazy entries are loaded before checking
    ensureLoaded (key);

    auto it = cache.find (key);
    if (it != cache.end())
        return ! it->second.maqamList.empty();

    return false;
}

const ApiDataCache::TuningData& ApiDataCache::getData (
    const juce::String& systemId, const juce::String& startingNote) const
{
    juce::ScopedLock sl (lock);
    static TuningData empty;
    const auto key = makeKey (systemId, startingNote);
    ensureLoaded (key);
    auto it = cache.find (key);
    return (it != cache.end()) ? it->second : empty;
}

void ApiDataCache::storeData (const juce::String& systemId,
                               const juce::String& startingNote,
                               TuningData data)
{
    juce::ScopedLock sl (lock);
    const auto key = makeKey (systemId, startingNote);
    lazyKeys.erase (key);
    cache[key] = std::move (data);
    saveEntryToDisk (key, cache[key]);
}

void ApiDataCache::updateLastChecked (const juce::String& systemId,
                                       const juce::String& startingNote)
{
    juce::ScopedLock sl (lock);
    const auto key = makeKey (systemId, startingNote);
    ensureLoaded (key);
    auto it = cache.find (key);
    if (it != cache.end())
    {
        it->second.lastChecked = juce::Time::getCurrentTime().toISO8601 (true);
        saveEntryToDisk (key, it->second);
    }
}

void ApiDataCache::updateMaqamList (const juce::String& systemId,
                                     const juce::String& startingNote,
                                     const std::vector<MaqamListEntry>& maqamList)
{
    juce::ScopedLock sl (lock);
    const auto key = makeKey (systemId, startingNote);
    ensureLoaded (key);
    auto it = cache.find (key);
    if (it != cache.end())
    {
        it->second.maqamList = maqamList;
        saveEntryToDisk (key, it->second);
    }
}

// ── Disk persistence ──────────────────────────────────────────────────────────

juce::File ApiDataCache::getCacheDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Tanghim")
               .getChildFile ("cache");
}

juce::File ApiDataCache::getCacheFile (const juce::String& key) const
{
    return getCacheDirectory().getChildFile (key.replaceCharacter (':', '_') + ".json");
}

juce::File ApiDataCache::getSystemsListFile() const
{
    return getCacheDirectory().getChildFile ("_tuning_systems.json");
}

bool ApiDataCache::ensureCacheDirectory() const
{
    const auto dir = getCacheDirectory();
    if (dir.isDirectory()) return true;

    // createDirectory() creates parent directories on all platforms (JUCE)
    const auto result = dir.createDirectory();
    if (result.failed())
    {
        DBG ("ApiDataCache: failed to create cache directory: " + dir.getFullPathName()
             + " — " + result.getErrorMessage());
        return false;
    }
    return true;
}

void ApiDataCache::loadFromDisk()
{
    juce::ScopedLock sl (lock);
    const auto dir = getCacheDirectory();
    if (! dir.isDirectory()) return;

    const auto startTime = juce::Time::getMillisecondCounterHiRes();

    // Load tuning systems list (small file, always loaded eagerly)
    const auto sysFile = getSystemsListFile();
    if (sysFile.existsAsFile())
    {
        const auto json = juce::JSON::parse (sysFile.loadFileAsString());
        if (json.isArray())
        {
            tuningSystemsList.clear();
            tuningSystemsList.reserve ((size_t) json.size());
            for (int i = 0; i < json.size(); ++i)
            {
                const auto& obj = json[i];
                if (obj.getDynamicObject() == nullptr) continue;
                const auto& p = obj.getDynamicObject()->getProperties();

                TuningSystem ts;
                auto get = [&] (const char* k) { const juce::var* v = p.getVarPointer(k); return v ? *v : juce::var(); };
                ts.id                    = get("id").toString();
                ts.displayName           = get("displayName").toString();
                ts.shortName             = get("shortName").toString();
                ts.yearStr               = get("yearStr").toString();
                ts.year                  = (int) get("year");
                ts.pitchClassesPerOctave = (int) get("pitchClassesPerOctave");
                ts.referenceFrequency    = (double) get("referenceFrequency");
                ts.version               = get("version").toString();

                const juce::var* sn = p.getVarPointer("startingNotes");
                if (sn && sn->isArray())
                    for (int j = 0; j < sn->size(); ++j)
                    {
                        const auto& e = (*sn)[j];
                        if (e.getDynamicObject())
                        {
                            const auto& ep = e.getDynamicObject()->getProperties();
                            const juce::var* sid = ep.getVarPointer("id");
                            const juce::var* sdn = ep.getVarPointer("displayName");
                            if (sid) ts.startingNoteIds.add(sid->toString());
                            if (sdn) ts.startingNoteDisplayNames.add(sdn->toString());
                        }
                    }

                if (ts.isValid()) tuningSystemsList.push_back (std::move (ts));
            }
            // Sort chronologically by year
            std::sort (tuningSystemsList.begin(), tuningSystemsList.end(),
                       [] (const TuningSystem& a, const TuningSystem& b)
               { return a.year != b.year ? a.year < b.year : a.yearStr < b.yearStr; });
            hasSystems = ! tuningSystemsList.empty();
        }
    }

    // Scan for tuning data files — record keys for lazy loading (no deserialization)
    // Filename format: systemId_startingNote.json (underscore separator)
    // Key format: systemId:startingNote (colon separator)
    // Only the LAST underscore is the separator — system IDs contain underscores too
    for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.json"))
    {
        if (f.getFileName().startsWith ("_")) continue;
        const auto name = f.getFileNameWithoutExtension();
        const auto lastUnderscore = name.lastIndexOfChar ('_');
        if (lastUnderscore <= 0) continue;  // skip malformed filenames
        const auto key = name.substring (0, lastUnderscore) + ":" + name.substring (lastUnderscore + 1);
        lazyKeys.insert (key);
    }

    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - startTime;
    DBG ("ApiDataCache::loadFromDisk: scanned " + juce::String ((int) lazyKeys.size())
         + " tuning entries in " + juce::String (elapsed, 1) + " ms (lazy)");
}

void ApiDataCache::clearAll()
{
    juce::ScopedLock sl (lock);
    cache.clear();
    lazyKeys.clear();
    tuningSystemsList.clear();
    hasSystems = false;
    getCacheDirectory().deleteRecursively();
}

bool ApiDataCache::isInMemory (const juce::String& systemId,
                                const juce::String& startingNote) const
{
    juce::ScopedLock sl (lock);
    return cache.count (makeKey (systemId, startingNote)) > 0;
}

void ApiDataCache::preload (const juce::String& systemId,
                             const juce::String& startingNote)
{
    const auto key = makeKey (systemId, startingNote);

    // Check under lock whether preloading is needed
    {
        juce::ScopedLock sl (lock);
        if (cache.count (key) > 0) return;         // already in memory
        if (lazyKeys.count (key) == 0) return;      // not on disk either
    }

    // Read + parse JSON outside the lock (expensive part)
    const auto startTime = juce::Time::getMillisecondCounterHiRes();
    const auto file = getCacheFile (key);
    TuningData loaded;
    bool ok = false;

    if (file.existsAsFile())
    {
        const auto json = juce::JSON::parse (file.loadFileAsString());
        if (json.getDynamicObject() != nullptr)
        {
            loaded = jsonToTuningData (json);
            ok = true;
        }
    }

    // Briefly lock to insert into cache
    {
        juce::ScopedLock sl (lock);
        lazyKeys.erase (key);
        if (ok)
            cache[key] = std::move (loaded);
    }

    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - startTime;
    DBG ("ApiDataCache::preload: deserialized '" + key + "' in "
         + juce::String (elapsed, 1) + " ms (background)");
}

// ── Lazy loading ─────────────────────────────────────────────────────────────

void ApiDataCache::ensureLoaded (const juce::String& key) const
{
    // Already in memory?
    if (cache.count (key) > 0) return;

    // Not a known lazy key?
    if (lazyKeys.count (key) == 0) return;

    const auto startTime = juce::Time::getMillisecondCounterHiRes();

    const auto file = getCacheFile (key);
    if (file.existsAsFile())
    {
        const auto json = juce::JSON::parse (file.loadFileAsString());
        if (json.getDynamicObject() != nullptr)
            cache[key] = jsonToTuningData (json);
    }
    lazyKeys.erase (key);

    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - startTime;
    DBG ("ApiDataCache::ensureLoaded: deserialized '" + key + "' in "
         + juce::String (elapsed, 1) + " ms");
}

// ── Incremental disk writes ──────────────────────────────────────────────────

void ApiDataCache::saveEntryToDisk (const juce::String& key, const TuningData& data) const
{
    if (! ensureCacheDirectory()) return;

    const auto startTime = juce::Time::getMillisecondCounterHiRes();
    getCacheFile (key).replaceWithText (juce::JSON::toString (tuningDataToJson (data)));
    const auto elapsed = juce::Time::getMillisecondCounterHiRes() - startTime;

    DBG ("ApiDataCache::saveEntryToDisk: wrote '" + key + "' in "
         + juce::String (elapsed, 1) + " ms");
}

void ApiDataCache::saveSystemsListToDisk() const
{
    if (! ensureCacheDirectory()) return;

    juce::Array<juce::var> arr;
    for (const auto& ts : tuningSystemsList)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id",          ts.id);
        obj->setProperty ("displayName", ts.displayName);
        obj->setProperty ("shortName",   ts.shortName);
        obj->setProperty ("year",        ts.year);
        obj->setProperty ("yearStr",     ts.yearStr);
        obj->setProperty ("pitchClassesPerOctave", ts.pitchClassesPerOctave);
        obj->setProperty ("referenceFrequency", ts.referenceFrequency);
        obj->setProperty ("version",     ts.version);

        juce::Array<juce::var> sn;
        for (int i = 0; i < ts.startingNoteIds.size(); ++i)
        {
            auto* snObj = new juce::DynamicObject();
            snObj->setProperty ("id", ts.startingNoteIds[i]);
            snObj->setProperty ("displayName", ts.startingNoteDisplayNames[i]);
            sn.add (juce::var (snObj));
        }
        obj->setProperty ("startingNotes", sn);
        arr.add (juce::var (obj));
    }
    getSystemsListFile().replaceWithText (juce::JSON::toString (juce::var (arr)));
}

// ── JSON serialisation helpers ────────────────────────────────────────────────

juce::var ApiDataCache::pitchClassToJson (const PitchClass& pc)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("noteName",         pc.noteName);
    obj->setProperty ("noteNameDisplay",  pc.noteNameDisplay);
    obj->setProperty ("englishName",      pc.englishName);
    obj->setProperty ("solfege",          pc.solfege);
    obj->setProperty ("abjadName",        pc.abjadName);
    obj->setProperty ("fraction",         pc.fraction);
    obj->setProperty ("cents",            pc.cents);
    obj->setProperty ("frequency",        pc.frequency);
    obj->setProperty ("pitchClassIndex",  pc.pitchClassIndex);
    obj->setProperty ("octave",           pc.octave);
    obj->setProperty ("midiNoteNumber",   pc.midiNoteNumber);
    obj->setProperty ("midiCentsDeviation", pc.midiCentsDeviation);
    obj->setProperty ("ipnReference",     pc.ipnReference);
    obj->setProperty ("version",          pc.version);
    return juce::var (obj);
}

PitchClass ApiDataCache::jsonToPitchClass (const juce::var& v)
{
    PitchClass pc;
    if (v.getDynamicObject() == nullptr) return pc;
    const auto& p = v.getDynamicObject()->getProperties();
    auto get = [&] (const char* k) { const juce::var* vv = p.getVarPointer(k); return vv ? *vv : juce::var(); };

    pc.noteName          = get("noteName").toString();
    pc.noteNameDisplay   = get("noteNameDisplay").toString();
    pc.englishName       = get("englishName").toString();
    pc.solfege           = get("solfege").toString();
    pc.abjadName         = get("abjadName").toString();
    pc.fraction          = get("fraction").toString();
    pc.cents             = (double) get("cents");
    pc.frequency         = (double) get("frequency");
    pc.pitchClassIndex   = (int) get("pitchClassIndex");
    pc.octave            = (int) get("octave");
    pc.midiNoteNumber    = (int) get("midiNoteNumber");
    pc.midiCentsDeviation = (double) get("midiCentsDeviation");
    pc.ipnReference      = get("ipnReference").toString();
    pc.version           = get("version").toString();
    return pc;
}

juce::var ApiDataCache::tuningDataToJson (const TuningData& data)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("tuningSystemVersion", data.tuningSystemVersion);
    obj->setProperty ("lastChecked",         data.lastChecked);

    juce::Array<juce::var> pcs;
    for (const auto& pc : data.pitchClasses)
        pcs.add (pitchClassToJson (pc));
    obj->setProperty ("pitchClasses", pcs);

    // Serialize MaqamList
    juce::Array<juce::var> maqamArr;
    for (const auto& mle : data.maqamList)
    {
        auto* mleObj = new juce::DynamicObject();
        mleObj->setProperty ("maqamId",       mle.maqamId);
        mleObj->setProperty ("maqamDisplay",   mle.maqamDisplay);
        mleObj->setProperty ("familyId",       mle.familyId);
        mleObj->setProperty ("familyDisplay",  mle.familyDisplay);
        mleObj->setProperty ("tonicId",        mle.tonicId);
        mleObj->setProperty ("tonicDisplay",   mle.tonicDisplay);

        // Degrees (ascending + descending + enriched fields)
        auto* degObj = new juce::DynamicObject();
        juce::Array<juce::var> asc, desc, ascEn, ascSol;
        for (const auto& n : mle.degrees.ascending)             asc.add (juce::var (n));
        for (const auto& n : mle.degrees.descending)            desc.add (juce::var (n));
        for (const auto& n : mle.degrees.ascendingEnglishNames) ascEn.add (juce::var (n));
        for (const auto& n : mle.degrees.ascendingSolfeges)     ascSol.add (juce::var (n));
        degObj->setProperty ("ascending",              asc);
        degObj->setProperty ("descending",             desc);
        degObj->setProperty ("ascendingEnglishNames",  ascEn);
        degObj->setProperty ("ascendingSolfeges",      ascSol);
        mleObj->setProperty ("degrees", juce::var (degObj));

        // Transpositions
        juce::Array<juce::var> transArr;
        for (const auto& t : mle.transpositions)
        {
            auto* tObj = new juce::DynamicObject();
            tObj->setProperty ("tonicId",      t.tonicId);
            tObj->setProperty ("tonicDisplay", t.tonicDisplay);

            auto* tdObj = new juce::DynamicObject();
            juce::Array<juce::var> tAsc, tDesc, tAscEn, tAscSol;
            for (const auto& n : t.degrees.ascending)             tAsc.add (juce::var (n));
            for (const auto& n : t.degrees.descending)            tDesc.add (juce::var (n));
            for (const auto& n : t.degrees.ascendingEnglishNames) tAscEn.add (juce::var (n));
            for (const auto& n : t.degrees.ascendingSolfeges)     tAscSol.add (juce::var (n));
            tdObj->setProperty ("ascending",              tAsc);
            tdObj->setProperty ("descending",             tDesc);
            tdObj->setProperty ("ascendingEnglishNames",  tAscEn);
            tdObj->setProperty ("ascendingSolfeges",      tAscSol);
            tObj->setProperty ("degrees", juce::var (tdObj));

            transArr.add (juce::var (tObj));
        }
        mleObj->setProperty ("transpositions", transArr);
        maqamArr.add (juce::var (mleObj));
    }
    obj->setProperty ("maqamList", maqamArr);

    return juce::var (obj);
}

ApiDataCache::TuningData ApiDataCache::jsonToTuningData (const juce::var& json)
{
    TuningData data;
    if (json.getDynamicObject() == nullptr) return data;
    const auto& p = json.getDynamicObject()->getProperties();
    auto get = [&] (const char* k) { const juce::var* v = p.getVarPointer(k); return v ? *v : juce::var(); };

    data.tuningSystemVersion = get("tuningSystemVersion").toString();
    data.lastChecked         = get("lastChecked").toString();

    const juce::var* pcs = p.getVarPointer("pitchClasses");
    if (pcs && pcs->isArray())
    {
        data.pitchClasses.reserve ((size_t) pcs->size());
        for (int i = 0; i < pcs->size(); ++i)
            data.pitchClasses.push_back (jsonToPitchClass ((*pcs)[i]));
    }

    // Deserialize MaqamList
    const juce::var* maqamArr = p.getVarPointer ("maqamList");
    if (maqamArr && maqamArr->isArray())
    {
        data.maqamList.reserve ((size_t) maqamArr->size());
        for (int i = 0; i < maqamArr->size(); ++i)
        {
            const auto& mleObj = (*maqamArr)[i];
            if (mleObj.getDynamicObject() == nullptr) continue;
            const auto& mp = mleObj.getDynamicObject()->getProperties();
            auto mGet = [&] (const char* k) { const juce::var* v = mp.getVarPointer (k); return v ? *v : juce::var(); };

            MaqamListEntry mle;
            mle.maqamId       = mGet ("maqamId").toString();
            mle.maqamDisplay  = mGet ("maqamDisplay").toString();
            mle.familyId      = mGet ("familyId").toString();
            mle.familyDisplay = mGet ("familyDisplay").toString();
            mle.tonicId       = mGet ("tonicId").toString();
            mle.tonicDisplay  = mGet ("tonicDisplay").toString();

            // Degrees
            const juce::var* degV = mp.getVarPointer ("degrees");
            if (degV && degV->getDynamicObject())
            {
                const auto& dp = degV->getDynamicObject()->getProperties();
                const juce::var* ascV = dp.getVarPointer ("ascending");
                if (ascV && ascV->isArray())
                {
                    mle.degrees.ascending.reserve ((size_t) ascV->size());
                    for (int j = 0; j < ascV->size(); ++j)
                        mle.degrees.ascending.push_back ((*ascV)[j].toString());
                }
                const juce::var* descV = dp.getVarPointer ("descending");
                if (descV && descV->isArray())
                {
                    mle.degrees.descending.reserve ((size_t) descV->size());
                    for (int j = 0; j < descV->size(); ++j)
                        mle.degrees.descending.push_back ((*descV)[j].toString());
                }
                const juce::var* ascEnV = dp.getVarPointer ("ascendingEnglishNames");
                if (ascEnV && ascEnV->isArray())
                {
                    mle.degrees.ascendingEnglishNames.reserve ((size_t) ascEnV->size());
                    for (int j = 0; j < ascEnV->size(); ++j)
                        mle.degrees.ascendingEnglishNames.push_back ((*ascEnV)[j].toString());
                }
                const juce::var* ascSolV = dp.getVarPointer ("ascendingSolfeges");
                if (ascSolV && ascSolV->isArray())
                {
                    mle.degrees.ascendingSolfeges.reserve ((size_t) ascSolV->size());
                    for (int j = 0; j < ascSolV->size(); ++j)
                        mle.degrees.ascendingSolfeges.push_back ((*ascSolV)[j].toString());
                }
            }

            // Transpositions
            const juce::var* transV = mp.getVarPointer ("transpositions");
            if (transV && transV->isArray())
            {
                mle.transpositions.reserve ((size_t) transV->size());
                for (int j = 0; j < transV->size(); ++j)
                {
                    const auto& tObj = (*transV)[j];
                    if (tObj.getDynamicObject() == nullptr) continue;
                    const auto& tp = tObj.getDynamicObject()->getProperties();
                    auto tGet = [&] (const char* k) { const juce::var* v = tp.getVarPointer (k); return v ? *v : juce::var(); };

                    MaqamTransposition trans;
                    trans.tonicId      = tGet ("tonicId").toString();
                    trans.tonicDisplay = tGet ("tonicDisplay").toString();

                    const juce::var* tdV = tp.getVarPointer ("degrees");
                    if (tdV && tdV->getDynamicObject())
                    {
                        const auto& tdp = tdV->getDynamicObject()->getProperties();
                        const juce::var* tAscV = tdp.getVarPointer ("ascending");
                        if (tAscV && tAscV->isArray())
                        {
                            trans.degrees.ascending.reserve ((size_t) tAscV->size());
                            for (int a = 0; a < tAscV->size(); ++a)
                                trans.degrees.ascending.push_back ((*tAscV)[a].toString());
                        }
                        const juce::var* tDescV = tdp.getVarPointer ("descending");
                        if (tDescV && tDescV->isArray())
                        {
                            trans.degrees.descending.reserve ((size_t) tDescV->size());
                            for (int a = 0; a < tDescV->size(); ++a)
                                trans.degrees.descending.push_back ((*tDescV)[a].toString());
                        }
                        const juce::var* tAscEnV = tdp.getVarPointer ("ascendingEnglishNames");
                        if (tAscEnV && tAscEnV->isArray())
                        {
                            trans.degrees.ascendingEnglishNames.reserve ((size_t) tAscEnV->size());
                            for (int a = 0; a < tAscEnV->size(); ++a)
                                trans.degrees.ascendingEnglishNames.push_back ((*tAscEnV)[a].toString());
                        }
                        const juce::var* tAscSolV = tdp.getVarPointer ("ascendingSolfeges");
                        if (tAscSolV && tAscSolV->isArray())
                        {
                            trans.degrees.ascendingSolfeges.reserve ((size_t) tAscSolV->size());
                            for (int a = 0; a < tAscSolV->size(); ++a)
                                trans.degrees.ascendingSolfeges.push_back ((*tAscSolV)[a].toString());
                        }
                    }
                    mle.transpositions.push_back (std::move (trans));
                }
            }
            data.maqamList.push_back (std::move (mle));
        }
    }

    return data;
}
