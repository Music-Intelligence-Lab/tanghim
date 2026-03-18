#include "NoteSliderBankComponent.h"

NoteSliderBankComponent::NoteSliderBankComponent()
{
    slotCentsOverrides.fill (0.0);
    hasSlotCentsOverride.fill (false);
    midiHitState.fill (false);

    for (auto& slider : sliders)
    {
        addChildComponent (slider);
        setupSliderCallbacks (slider);
    }
}

void NoteSliderBankComponent::setupSliderCallbacks (NoteSliderComponent& slider)
{
    slider.onVariantSelect = [this] (int ci, int vi) { if (onVariantSelect) onVariantSelect (ci, vi); };
    slider.onCentsDrag     = [this] (int ci, double c) { if (onCentsDrag) onCentsDrag (ci, c); };
    slider.onCentsDragEnd  = [this] (int ci, double c) { if (onCentsDragEnd) onCentsDragEnd (ci, c); };
    slider.onNoteCentsDrag = [this] (int mn, double c) { if (onNoteCentsDrag) onNoteCentsDrag (mn, c); };
    slider.onNoteCentsDragEnd = [this] (int mn, double c) { if (onNoteCentsDragEnd) onNoteCentsDragEnd (mn, c); };
    slider.onGestureStart  = [this] (int ci) { if (onGestureStart) onGestureStart (ci); };
    slider.onGestureEnd    = [this] (int ci) { if (onGestureEnd) onGestureEnd (ci); };
}

// ── Configuration ─────────────────────────────────────────────────────────────

void NoteSliderBankComponent::setTuningState (const ActiveTuningState& state)
{
    tuningState = state;
    hasSlotCentsOverride.fill (false);
    updateVisibleSliders();
}

void NoteSliderBankComponent::setScrollPosition (double startMidi)
{
    if (scrollPosition != startMidi)
    {
        scrollPosition = startMidi;
        updateVisibleSliders();
    }
}

void NoteSliderBankComponent::setMaqamInfo (const std::set<int>& degrees,
                                            int tonicIdx, int tonicMidi,
                                            const std::set<int>& modified,
                                            const std::map<int, juce::String>& degreePaoNames)
{
    maqamDegreeIndices  = degrees;
    maqamTonicIndex     = tonicIdx;
    maqamTonicMidi      = tonicMidi;
    modifiedSlots       = modified;
    maqamDegreePaoNames = degreePaoNames;
    computeHeptInfo();
    updateVisibleSliders();
}

void NoteSliderBankComponent::setDegreeIpnMap (const std::array<juce::String, 12>& ipnMap)
{
    degreeIpnMap = ipnMap;
    // Recompute hept info — IPN letters determine white key mapping
    if (heptEnabled && ! maqamDegreeIndices.empty())
        computeHeptInfo();
    updateVisibleSliders();
}

void NoteSliderBankComponent::setDegreeSolfegeMap (const std::array<juce::String, 12>& solMap)
{
    degreeSolfegeMap = solMap;
    updateVisibleSliders();
}

void NoteSliderBankComponent::setNoteNames (const std::map<int, std::map<int, juce::String>>& names)
{
    noteNames = names;
    updateVisibleSliders();
}

void NoteSliderBankComponent::setHeptEnabled (bool enabled)
{
    if (heptEnabled != enabled)
    {
        heptEnabled = enabled;
        computeHeptInfo();
        updateVisibleSliders();
    }
}

void NoteSliderBankComponent::updateSlotCents (int chromaticIndex, double cents)
{
    if (chromaticIndex >= 0 && chromaticIndex < 12)
    {
        slotCentsOverrides[(size_t) chromaticIndex] = cents;
        hasSlotCentsOverride[(size_t) chromaticIndex] = true;

        // Repaint only affected visible sliders
        const int renderStart = (int) scrollPosition;
        const int visCount = getVisibleSliderCount();
        for (int i = 0; i < visCount + 2 && renderStart + i < 128; ++i)
        {
            const int midi = renderStart + i;
            if (midi % 12 == chromaticIndex)
            {
                sliders[(size_t) midi].setCentsOverride (cents);
                sliders[(size_t) midi].repaint();
            }
        }
    }
}

void NoteSliderBankComponent::updateMidiActivity (const std::vector<int>& onsVec,
                                                  const std::vector<int>& offsVec)
{
    // React: visibleCount <= 12 → pitch class mode (all octaves of same chromatic light up)
    //        visibleCount > 12  → per-note mode (only exact MIDI note lights up)
    const bool pitchClassMode = getVisibleSliderCount() <= 12;

    // Update per-note state first (source of truth)
    for (int note : onsVec)
        if (note >= 0 && note < 128)
            midiHitState[(size_t) note] = true;
    for (int note : offsVec)
        if (note >= 0 && note < 128)
            midiHitState[(size_t) note] = false;

    if (pitchClassMode)
    {
        // In pitch class mode, a slider lights up if ANY note of the same chromatic index is active
        for (int note : onsVec)
        {
            if (note < 0 || note >= 128) continue;
            int ci = note % 12;
            // Light up all visible sliders with this chromatic index
            for (int n = 0; n < 128; ++n)
            {
                if (n % 12 == ci && sliders[(size_t) n].isVisible())
                    sliders[(size_t) n].setMidiHit (true);
            }
        }
        for (int note : offsVec)
        {
            if (note < 0 || note >= 128) continue;
            int ci = note % 12;
            // Only turn off if NO other notes of this pitch class are active
            bool anyActive = false;
            for (int n = 0; n < 128; ++n)
            {
                if (n % 12 == ci && midiHitState[(size_t) n])
                { anyActive = true; break; }
            }
            if (! anyActive)
            {
                for (int n = 0; n < 128; ++n)
                {
                    if (n % 12 == ci && sliders[(size_t) n].isVisible())
                        sliders[(size_t) n].setMidiHit (false);
                }
            }
        }
    }
    else
    {
        // Per-note mode: only exact MIDI note
        for (int note : onsVec)
            if (note >= 0 && note < 128)
                sliders[(size_t) note].setMidiHit (true);
        for (int note : offsVec)
            if (note >= 0 && note < 128)
                sliders[(size_t) note].setMidiHit (false);
    }
}

int NoteSliderBankComponent::getVisibleSliderCount() const
{
    return std::max (1, getWidth() / Theme::kSlotWidthPx);
}

void NoteSliderBankComponent::resized()
{
    updateVisibleSliders();
}

// ── Hept info computation ─────────────────────────────────────────────────────

void NoteSliderBankComponent::computeHeptInfo()
{
    heptMuted.clear();
    heptSourceKeyMap.clear();

    if (! heptEnabled || maqamDegreeIndices.empty()) return;

    std::set<int> reachable;

    // Black keys always reachable (delta 0)
    for (int bk : BLACK_KEYS)
        reachable.insert (bk);

    // Degrees reachable; IPN first letter = source white key
    static const std::map<char, int> WHITE_KEY_MAP = {
        {'C', 0}, {'D', 2}, {'E', 4}, {'F', 5}, {'G', 7}, {'A', 9}, {'B', 11}
    };

    for (int ci : maqamDegreeIndices)
    {
        reachable.insert (ci);
        juce::String ipn = degreeIpnMap[(size_t) ci];

        if (ipn.isNotEmpty())
        {
            char letter = ipn[0];
            auto it = WHITE_KEY_MAP.find (letter);
            if (it != WHITE_KEY_MAP.end() && it->second != ci)
                heptSourceKeyMap[ci] = juce::String::charToString (letter);
        }
    }

    for (int i = 0; i < 12; ++i)
    {
        if (reachable.find (i) == reachable.end())
            heptMuted.insert (i);
    }
}

// ── Update visible sliders ────────────────────────────────────────────────────

void NoteSliderBankComponent::updateVisibleSliders()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;

    const int renderStart = juce::jlimit (0, 127, (int) scrollPosition);
    const double fractional = scrollPosition - renderStart;
    const int pixelOffset = (int) (fractional * Theme::kSlotWidthPx);
    const int visCount = getVisibleSliderCount();
    const int renderCount = std::min (visCount + 2, 128 - renderStart);

    // Hide all sliders first (only visible ones will be shown)
    for (auto& slider : sliders)
        slider.setVisible (false);

    for (int i = 0; i < renderCount; ++i)
    {
        const int midi = renderStart + i;
        if (midi < 0 || midi >= 128) continue;

        const int ci = midi % 12;
        const int octave = midi / 12 - 1;
        auto& slider = sliders[(size_t) midi];
        auto& slot = tuningState.slots[(size_t) ci];

        // Position — CSS: padding 8px 0 (8px top/bottom inside bank)
        const int bankPad = 8;
        const int x = Theme::kBankLeftOffsetPx + i * Theme::kSlotWidthPx - pixelOffset;
        slider.setBounds (x, bankPad, Theme::kSlotWidthPx, getHeight() - bankPad * 2);

        // Resolve effective variant index
        const int noteOverride = tuningState.perNoteVariantOverrides[(size_t) midi];
        const int effectiveIdx = (noteOverride >= 0)
                                 ? juce::jlimit (0, std::max (0, slot.variantCount() - 1), noteOverride)
                                 : slot.selectedIndex;
        const bool hasOverride = noteOverride >= 0;

        // Set slot data
        slider.setSlotData (slot, ci, midi, effectiveIdx);

        // Maqam state
        bool isDegree = maqamDegreeIndices.count (ci) > 0;
        bool inHomeOctave = maqamTonicMidi >= 0 && midi >= maqamTonicMidi && midi < maqamTonicMidi + 12;
        bool isModified = modifiedSlots.count (ci) > 0;

        // Find maqam variant index (by PAO name match)
        int maqamVarIdx = -1;
        auto paoIt = maqamDegreePaoNames.find (ci);
        if (paoIt != maqamDegreePaoNames.end())
        {
            for (int v = 0; v < slot.variantCount(); ++v)
            {
                if (slot.variants[(size_t) v].noteName == paoIt->second)
                {
                    maqamVarIdx = v;
                    break;
                }
            }
        }

        slider.setMaqamState (isDegree && inHomeOctave,
                              isDegree && ! inHomeOctave,
                              midi == maqamTonicMidi,
                              ci == maqamTonicIndex && midi != maqamTonicMidi,
                              isModified,
                              maqamVarIdx);

        // Hept state
        bool isWhiteKeyStd = (ci == 0 || ci == 2 || ci == 4 || ci == 5
                              || ci == 7 || ci == 9 || ci == 11);
        bool whiteKey = heptEnabled ? isDegree : isWhiteKeyStd;
        bool heptIsMuted = heptEnabled && heptMuted.count (ci) > 0;
        juce::String srcKey;
        auto srcIt = heptSourceKeyMap.find (ci);
        if (srcIt != heptSourceKeyMap.end())
            srcKey = srcIt->second;
        slider.setHeptState (heptIsMuted, heptEnabled ? srcKey : juce::String(), whiteKey);

        // Labels — IPN: prefer degree map (context-aware from maqam detail),
        // fall back to selected variant's ipnReference (from pitch class data)
        juce::String ipn = degreeIpnMap[(size_t) ci];
        if (ipn.isEmpty() && effectiveIdx >= 0 && effectiveIdx < slot.variantCount())
            ipn = slot.variants[(size_t) effectiveIdx].ipnReference;
        if (ipn.isEmpty()) ipn = IPN_NAMES[ci];
        juce::String ipnLabel = ipn + juce::String (octave);

        juce::String solfege = degreeSolfegeMap[(size_t) ci];
        if (solfege.isEmpty())
        {
            // Fall back to variant solfege
            if (effectiveIdx >= 0 && effectiveIdx < slot.variantCount())
                solfege = slot.variants[(size_t) effectiveIdx].solfege;
            if (solfege.isEmpty()) solfege = juce::String::charToString (0x2014);  // em-dash
        }

        juce::String paoName;
        auto ciIt = noteNames.find (ci);
        if (ciIt != noteNames.end())
        {
            auto octIt = ciIt->second.find (octave);
            if (octIt != ciIt->second.end())
                paoName = octIt->second;
        }
        if (paoName.isEmpty()) paoName = juce::String::charToString (0x2014);

        slider.setLabels (ipnLabel, solfege, paoName);

        // Override state
        bool hasPerNoteCents = tuningState.hasPerNoteCentsOverride (midi);
        double perNoteCtsVal = hasPerNoteCents
                               ? tuningState.perNoteCentsOverrides[(size_t) midi] : 0.0;
        slider.setOverrideState (hasOverride, hasPerNoteCents, perNoteCtsVal);

        // Cents override from DAW automation
        if (hasSlotCentsOverride[(size_t) ci])
            slider.setCentsOverride (slotCentsOverrides[(size_t) ci]);
        else
            slider.clearCentsOverride();

        // MIDI hit state
        slider.setMidiHit (midiHitState[(size_t) midi]);

        slider.setVisible (true);
    }
}
