import { useState, useEffect, useCallback, useRef } from 'react'
import type { TuningState, TuningSystem, MaqamListEntry, MtsStatusUpdate } from './types'
import { useJuceBridge, useJuceEvent } from './hooks/useJuceBridge'
import { useVisibleSliderCount } from './hooks/useVisibleSliderCount'
import { SLOT_WIDTH_PX } from './constants'
import './App.css'

import TuningSystemSelector from './components/TuningSystemSelector'
import ReferenceFreqControl from './components/ReferenceFreqControl'
import OutputModeSelector from './components/OutputModeSelector'
import MaqamSelector from './components/MaqamSelector'
import MaqamPresetBar from './components/MaqamPresetBar'
import RangeScroller from './components/RangeScroller'
import NoteSliderBank from './components/NoteSliderBank'
import StatusBar from './components/StatusBar'

const EMPTY_STATE: TuningState = {
  systemId: '',
  startingNote: '',
  isMtsTransmitter: false,
  oscillatorEnabled: false,
  heptEnabled: false,
  mtsReceivers: 0,
  mtsNativeCount: 0,
  mpeCount: 0,
  monoPbCount: 0,
  pluginVersion: '',
  buildTimestamp: '',
  referenceFreqCents: 0,
  referenceFreqHz: 440,
  referenceDefaultHz: 440,
  referenceNoteName: '',
  slots: Array.from({ length: 12 }, (_, i) => ({
    ipnRef: ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B'][i],
    selectedIndex: 0,
    isLocked: true,
    variants: [],
    centsOffset: 0,
  })),
  presets: Array.from({ length: 16 }, () => ({
    isAssigned: false, maqamId: '', maqamDisplay: '', tonicIpn: '', tonicSolfege: '',
    baseMaqamId: '', isTransposed: false, tonicNote: '', setIndex: -1, sliderPositions: [],
    centsOffsets: [],
    degreeNames: [],
    tuningSystemId: '',
    startingNote: '',
  })),
  noteNames: {},
  perNoteOverrides: {},
  paoNameMap: {},
  paoOrder: [],
  paoNameInfo: {},
  degreeIpnMap: {},
  degreeSolfegeMap: {},
  selectedMaqamId: '',
  transpositionIndex: -1,
  activePresetIndex: -1,
  startMidi: 48,
  degreeNames: [],
  sessionRecallInProgress: false,
  hasRecalledSessionState: false,
}

const EMPTY_SET = new Set<number>()

/** Compute which chromatic indices are maqam degrees.
 *  Uses paoNameMap (idName → chromaticIndex) from the tuning state,
 *  which covers ALL pitch classes across all octaves — including
 *  register-specific names like kirdan (C5) and muhayyar (A4). */
function computeMaqamDegreeIndices(
  ascendingNames: string[],
  paoNameMap: Record<string, number>
): Set<number> {
  const result = new Set<number>()
  for (const name of ascendingNames) {
    const ci = paoNameMap[name]
    if (ci !== undefined) result.add(ci)
  }
  return result
}

/** Build a map of chromatic index → expected PAO name for maqam degrees.
 *  Used for modification tracking across tuning systems — PAO names are the
 *  constant that allows comparing the same maqam in different tunings. */
function buildDegreePaoNameMap(
  ascendingNames: string[],
  paoNameMap: Record<string, number>
): Map<number, string> {
  const result = new Map<number, string>()
  for (const name of ascendingNames) {
    const ci = paoNameMap[name]
    // Use first occurrence only — ascending degrees start from the base register
    // (e.g. "rast" at C3), and the octave note (e.g. "kirdan" at C4) comes later
    // with the same chromatic index. Slot variants only cover MIDI 48-59 (base register).
    if (ci !== undefined && !result.has(ci)) result.set(ci, name)
  }
  return result
}

/**
 * Find the MIDI note and chromatic index for a tonic by its display name.
 *
 * The noteNames map (chromaticIndex → { octave → PAO display name }) covers
 * all octaves with register-specific names (e.g. "dūgāh" at A3, "muḥayyar"
 * at A4). This is the authoritative source for resolving a tonic to its
 * correct MIDI position.
 */
function findTonicMidi(
  tonicDisplay: string,
  noteNames: Record<string, Record<string, string>>
): { midi: number; chromaticIndex: number } | undefined {
  for (const [ciStr, octaveMap] of Object.entries(noteNames)) {
    const ci = parseInt(ciStr, 10)
    for (const [octStr, displayName] of Object.entries(octaveMap)) {
      if (displayName === tonicDisplay) {
        const oct = parseInt(octStr, 10)
        return { midi: (oct + 1) * 12 + ci, chromaticIndex: ci }
      }
    }
  }
  return undefined
}

/** Center a maqam's octave (13 notes: tonic through its octave) within the visible slider viewport.
 *  Uses fractional visibleCount for smooth "curtain opening" effect. */
function centerMaqamOctave(tonicMidi: number, visibleCount: number): number {
  const padding = Math.max(0, Math.floor((visibleCount - 13) / 2))
  return Math.max(0, Math.min(128 - visibleCount, tonicMidi - padding))
}

export default function App() {
  const [tuningSystems, setTuningSystems] = useState<TuningSystem[]>([])
  const [tuningState, setTuningState]     = useState<TuningState>(EMPTY_STATE)
  const [status, setStatus]               = useState<string>('')
  const statusTimer = useRef<ReturnType<typeof setTimeout>>()

  // ── Maqam selector state ────────────────────────────────────────────
  const [maqamList, setMaqamList] = useState<MaqamListEntry[]>([])
  const [selectedMaqamId, setSelectedMaqamId] = useState('')
  const [selectedTransIdx, setSelectedTransIdx] = useState(-1)  // -1 = base
  const [activePresetIndex, setActivePresetIndex] = useState(-1)
  const [maqamDegreeIndices, setMaqamDegreeIndices] = useState<Set<number>>(EMPTY_SET)
  const [maqamTonicIndex, setMaqamTonicIndex] = useState(-1)  // chromatic index of tonic, -1 = none
  const [maqamTonicMidi, setMaqamTonicMidi] = useState(-1)   // MIDI note of tonic, -1 = none
  const [isMaqamModified, setIsMaqamModified] = useState(false)  // true when slider adjusted while maqam selected
  const [modifiedSlots, setModifiedSlots] = useState<Set<number>>(new Set())  // chromatic indices of modified sliders
  const [maqamDegreePaoNames, setMaqamDegreePaoNames] = useState<Map<number, string>>(new Map())  // chromatic index → expected PAO name
  const maqamListRequested = useRef(false)
  const afterSystemSwitchRef = useRef<((state: TuningState) => void | false) | null>(null)  // callback to run after tuning system loads; return false to keep for next event
  const presetIndexOverrideRef = useRef<number | null>(null)  // guards preset index from async event overwrite
  const preSystemSwitchPresetRef = useRef(-1)  // captures active preset before system/note switch for re-apply check
  const systemSwitchPendingRef = useRef(false)  // true during system/note switch, blocks syncMaqamStateFromCpp from restoring preset

  // Refs for modification tracking — ensures callbacks always have latest values
  // without stale closures. Updated both during render AND immediately in handlers.
  const maqamDegreeIndicesRef = useRef<Set<number>>(EMPTY_SET)
  const selectedMaqamIdRef = useRef('')
  const isMaqamModifiedRef = useRef(false)

  const showStatus = useCallback((msg: string, durationMs = 4000) => {
    setStatus(msg)
    clearTimeout(statusTimer.current)
    if (msg) {
      statusTimer.current = setTimeout(() => setStatus(''), durationMs)
    }
  }, [])

  const bridge = useJuceBridge()
  const didInit = useRef(false)
  const prevRecalledRef = useRef(false)

  /** Sync JS maqam state from C++ tuning state (used on session recall and MIDI preset triggers). */
  const syncMaqamStateFromCpp = useCallback((state: TuningState, centerOnMaqam = false) => {
    // Preserve JS-side maqam state if:
    // 1. User has modified the maqam (slider adjusted), OR
    // 2. We already have a maqam selected and C++ is sending empty state (stale async event)
    // C++ clears currentMaqamId when sliders are manually adjusted, but we want to keep
    // showing the modified maqam with an asterisk. Also, async APVTS callbacks can send
    // stale events after handleMaqamSelect has already set the state.
    // If handleMaqamSelect set a preset override (maqam matches existing preset),
    // consume it instead of using C++'s -1 (C++ doesn't know about the match).
    const presetOverride = presetIndexOverrideRef.current
    // Consume the override only when C++ agrees (i.e. preset was applied successfully).
    // Async events like fetchAndApplyMaqamDetail send activePresetIndex=-1 because
    // C++ applyMaqam clears currentActivePresetIdx — keep the override for those.
    if (presetOverride !== null && (state.activePresetIndex ?? -1) === presetOverride)
      presetIndexOverrideRef.current = null

    // During a system/note switch, don't restore C++ activePresetIndex —
    // the maqam list useEffect will handle preset re-apply/deactivation
    // after compatibility is confirmed.
    const pendingSwitch = systemSwitchPendingRef.current

    if (!state.selectedMaqamId && (isMaqamModifiedRef.current || selectedMaqamIdRef.current)
        && state.systemId) {
      // Don't overwrite maqam selection — keep the current state
      // Only sync preset index (it might have changed)
      // Exception: if systemId is empty (cache cleared), always reset
      if (!pendingSwitch) setActivePresetIndex(presetOverride ?? state.activePresetIndex ?? -1)
      return
    }

    const newMaqamId = state.selectedMaqamId || ''
    setSelectedMaqamId(newMaqamId)
    selectedMaqamIdRef.current = newMaqamId  // Update ref immediately
    setSelectedTransIdx(state.transpositionIndex ?? -1)
    if (!pendingSwitch) setActivePresetIndex(presetOverride ?? state.activePresetIndex ?? -1)

    if (state.degreeNames && state.degreeNames.length > 0) {
      const degreeIndices = computeMaqamDegreeIndices(state.degreeNames, state.paoNameMap)
      setMaqamDegreeIndices(degreeIndices)
      maqamDegreeIndicesRef.current = degreeIndices  // Update ref immediately
      // Build PAO name map for modification tracking — essential for cyan thumb + asterisk
      setMaqamDegreePaoNames(buildDegreePaoNameMap(state.degreeNames, state.paoNameMap))
      const tonicName = state.degreeNames[0]
      const tonicCi = state.paoNameMap[tonicName]
      if (tonicCi !== undefined) {
        setMaqamTonicIndex(tonicCi)
        // Find proper tonic MIDI using noteNames
        const tonic = findTonicMidi(
          // Get display name from paoNameInfo or use the id
          Object.entries(state.noteNames[String(tonicCi)] || {}).find(([, name]) =>
            state.paoNameMap[name] === tonicCi
          )?.[1] || tonicName,
          state.noteNames
        )
        const tonicMidi = tonic?.midi ?? (tonicCi + 48)
        setMaqamTonicMidi(tonicMidi)

        // Center viewport on maqam when requested (e.g., MIDI preset trigger)
        if (centerOnMaqam) {
        }
      }
    } else if (!isMaqamModifiedRef.current && !selectedMaqamIdRef.current
               || !state.systemId) {
      // Clear maqam state when:
      // 1. No modification and no maqam selected (prevents stale async events from clearing), OR
      // 2. System is gone (cache cleared) — always reset
      if (!state.systemId) isMaqamModifiedRef.current = false
      setMaqamDegreeIndices(EMPTY_SET)
      maqamDegreeIndicesRef.current = EMPTY_SET  // Update ref immediately
      setMaqamDegreePaoNames(new Map())
      setMaqamTonicIndex(-1)
      setMaqamTonicMidi(-1)
      // Restore scroll position from C++ only on session recall — not during system switch
      // (handleSystemSelect already centered on C3) or preset deactivation (sticky viewport)
      if (state.startMidi !== undefined && !stickyViewportRef.current && !pendingSwitch)
        setStartMidi(state.startMidi)
    }
  }, [])

  /** Auto-select Ibn Sina as default, falling back to first system. */
  const autoSelectSystem = useCallback((systems: TuningSystem[]) => {
    if (systems.length === 0) return
    const preferred = systems.find(s => s.id === 'ibnsina_1037') ?? systems[0]
    const note = preferred.startingNotes[0]?.id ?? ''
    showStatus('Loading ' + (preferred.shortName || preferred.id) + '…', 10000)
    bridge.selectTuningSystem(preferred.id, note)
  }, [bridge, showStatus])

  // ── Initialize on mount ───────────────────────────────────────────────────
  useEffect(() => {
    let cancelled = false
    Promise.all([
      bridge.getTuningSystems(),
      bridge.getCurrentState()
    ]).then(([systems, state]) => {
      if (cancelled) return
      if (systems.length > 0) setTuningSystems(systems)
      if (state) setTuningState(state)

      if (state?.hasRecalledSessionState) {
        // Session recall already completed before editor opened
        didInit.current = true
        prevRecalledRef.current = true
        syncMaqamStateFromCpp(state)
      } else if (state?.sessionRecallInProgress) {
        // Session recall in progress — wait for tuningStateChanged event
        didInit.current = true
      } else if (state?.systemId) {
        // Saved settings but no active load — trigger load
        didInit.current = true
        bridge.selectTuningSystem(state.systemId, state.startingNote)
      } else if (systems.length > 0) {
        // No saved settings — auto-select
        didInit.current = true
        autoSelectSystem(systems)
      }
      // Else: no systems yet, will be handled by tuningSystemsLoaded event
    })
    return () => { cancelled = true }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [])

  // ── Listen for C++ push events ────────────────────────────────────────────
  const onTuningSystemsLoaded = useCallback((data: unknown) => {
    if (!Array.isArray(data)) return
    const systems = data as TuningSystem[]
    setTuningSystems(systems)
    // Auto-select if mount effect couldn't (no systems were available at mount time)
    if (!didInit.current && systems.length > 0) {
      didInit.current = true
      autoSelectSystem(systems)
    }
  }, [autoSelectSystem])

  const onTuningStateChanged = useCallback((data: unknown) => {
    if (!data || typeof data !== 'object') return
    const state = data as TuningState
    setTuningState(state)

    // Mark the loaded system+note as cached in the tuning systems list
    if (state.systemId && state.startingNote) {
      setTuningSystems(prev => prev.map(sys =>
        sys.id !== state.systemId ? sys : {
          ...sys,
          startingNotes: sys.startingNotes.map(n =>
            n.id === state.startingNote ? { ...n, isCached: true } : n
          )
        }
      ))
    }

    // Determine if this is a session recall (don't center) or a live change (center on maqam)
    // Session recall: first time hasRecalledSessionState becomes true, or sessionRecallInProgress
    const isSessionRecall = state.sessionRecallInProgress ||
      (state.hasRecalledSessionState && !prevRecalledRef.current)
    const shouldCenter = !isSessionRecall && state.degreeNames && state.degreeNames.length > 0

    // Always sync maqam/preset state from C++ — this handles both:
    // - Session recall on startup
    // - DAW automation changes (preset, slider, etc.)
    // C++ is the canonical source of truth; JS should reflect its state.
    syncMaqamStateFromCpp(state, shouldCenter)
    if (state.hasRecalledSessionState) prevRecalledRef.current = true

    // Execute pending callback after tuning system switch (used for modified preset loading + file load).
    // Returns true to consume (remove) the callback, false to keep it for next event.
    if (afterSystemSwitchRef.current) {
      const result = afterSystemSwitchRef.current(state)
      if (result !== false) afterSystemSwitchRef.current = null
    }
  }, [syncMaqamStateFromCpp])

  const onStatusMessage = useCallback((data: unknown) => {
    if (typeof data === 'string') showStatus(data)
  }, [showStatus])

  const onMaqamListLoaded = useCallback((data: unknown) => {
    if (Array.isArray(data)) setMaqamList(data as MaqamListEntry[])
  }, [])

  const onMtsStatusChanged = useCallback((data: unknown) => {
    if (!data || typeof data !== 'object') return
    const update = data as MtsStatusUpdate
    setTuningState(prev => ({
      ...prev,
      isMtsTransmitter: update.isMtsTransmitter,
      mtsNativeCount: update.mtsNativeCount,
      mpeCount: update.mpeCount,
      monoPbCount: update.monoPbCount,
    }))
  }, [])

  // Lightweight slot cents update from DAW automation (avoids full state rebuild)
  const onSlotCentsChanged = useCallback((data: unknown) => {
    if (!data || typeof data !== 'object') return
    const { index, cents } = data as { index: number; cents: number }
    if (typeof index !== 'number' || typeof cents !== 'number') return
    setTuningState(prev => {
      const slots = [...prev.slots]
      if (index >= 0 && index < 12) {
        slots[index] = { ...slots[index], centsOffset: cents }
      }
      return { ...prev, slots }
    })
  }, [])

  // ── Fetch maqam list once after first tuning state arrives ──────────────
  useEffect(() => {
    if (tuningState.systemId && !maqamListRequested.current) {
      maqamListRequested.current = true
      bridge.getMaqamList().then(list => {
        if (list.length > 0) setMaqamList(list)
      })
    }
  }, [tuningState.systemId, bridge])

  // ── Slider bank layout ───────────────────────────────────────────────────
  const [startMidi, setStartMidi] = useState(48) // default C3
  const bankContainerRef = useRef<HTMLDivElement>(null)
  const { count: visibleCount, widthPx: bankWidthPx } = useVisibleSliderCount(bankContainerRef)

  // Fractional visible count for smooth curtain effect
  const fractionalVisibleCount = bankWidthPx / SLOT_WIDTH_PX

  // Sticky viewport: when 24+ sliders visible and user has manually dragged the RangeScroller,
  // don't auto-center on maqam selection. Reset when viewport shrinks below 24.
  const stickyViewportRef = useRef(false)

  const handleRangeScrollerChange = useCallback((midi: number) => {
    if (fractionalVisibleCount >= 24) stickyViewportRef.current = true
    setStartMidi(midi)
  }, [fractionalVisibleCount])

  // Keep maqam's octave (or default C3 octave) centered during resize (smooth curtain effect)
  // Math.floor in centerMaqamOctave ensures tonic is flush at minimum width
  useEffect(() => {
    if (fractionalVisibleCount < 24) stickyViewportRef.current = false
    if (stickyViewportRef.current) return
    const tonicMidi = maqamTonicMidi >= 0 ? maqamTonicMidi : 48
    setStartMidi(centerMaqamOctave(tonicMidi, fractionalVisibleCount))
  }, [fractionalVisibleCount, maqamTonicMidi])

  // ── MIDI activity (note on / note off tracking) ─────────────────────────
  // Stored in a ref — NOT React state — so MIDI events never trigger re-renders.
  // The gold thumb glow is applied via direct DOM classList manipulation using
  // data-midi attributes on each NoteSlider element.
  const midiActiveNotesRef = useRef(new Set<number>())
  const visibleCountRef = useRef(visibleCount)
  visibleCountRef.current = visibleCount

  // Check if any note of the given pitch class is currently active
  const isPitchClassActive = (chromaticIndex: number): boolean => {
    for (const midi of midiActiveNotesRef.current) {
      if (midi % 12 === chromaticIndex) return true
    }
    return false
  }

  const onMidiActivity = useCallback((data: unknown) => {
    if (!data || typeof data !== 'object') return
    const { on, off } = data as { on?: number[]; off?: number[] }
    const onNotes  = Array.isArray(on)  ? on  : []
    const offNotes = Array.isArray(off) ? off : []
    if (!onNotes.length && !offNotes.length) return

    // Update ref first — this is the source of truth for the final state.
    // When rapid repeated notes cause the same note to appear in BOTH on[] and off[],
    // the ref resolves to the correct final state (off wins, since it's processed last).
    for (const n of onNotes)  midiActiveNotesRef.current.add(n)
    for (const n of offNotes) midiActiveNotesRef.current.delete(n)

    const usePitchClassMode = visibleCountRef.current <= 12

    // Apply final state to DOM — only mutate if the element's class doesn't already
    // match the ref. This eliminates redundant style recalcs when rapid repeated notes
    // cause the same note to appear in both on[] and off[] within a single timer tick.
    for (const n of onNotes) {
      if (!midiActiveNotesRef.current.has(n)) continue // cancelled by a later off
      if (usePitchClassMode) {
        // Highlight ALL visible sliders with the same chromatic index
        document.querySelectorAll('.note-slider .thumb').forEach(thumb => {
          const slider = thumb.closest('.note-slider')
          const midi = slider?.getAttribute('data-midi')
          if (midi && parseInt(midi) % 12 === n % 12) {
            if (!thumb.classList.contains('midi-hit')) thumb.classList.add('midi-hit')
          }
        })
      } else {
        const thumb = document.querySelector(`[data-midi="${n}"] .thumb`)
        if (thumb && !thumb.classList.contains('midi-hit')) thumb.classList.add('midi-hit')
      }
    }
    for (const n of offNotes) {
      if (midiActiveNotesRef.current.has(n)) continue // re-added by a later on
      if (usePitchClassMode) {
        // Only remove highlight if NO other notes of this pitch class are active
        const chromaticIndex = n % 12
        if (!isPitchClassActive(chromaticIndex)) {
          document.querySelectorAll('.note-slider .thumb').forEach(thumb => {
            const slider = thumb.closest('.note-slider')
            const midi = slider?.getAttribute('data-midi')
            if (midi && parseInt(midi) % 12 === chromaticIndex) {
              if (thumb.classList.contains('midi-hit')) thumb.classList.remove('midi-hit')
            }
          })
        }
      } else {
        const thumb = document.querySelector(`[data-midi="${n}"] .thumb`)
        if (thumb && thumb.classList.contains('midi-hit')) thumb.classList.remove('midi-hit')
      }
    }
  }, [])

  // Sync MIDI hit state when sliders scroll into view (new DOM elements
  // won't have the class applied yet). Runs after React commits to DOM.
  useEffect(() => {
    const start = Math.floor(startMidi)
    const usePitchClassMode = visibleCount <= 12

    for (let i = 0; i < visibleCount + 1 && start + i < 128; i++) {
      const midi = start + i
      const thumb = document.querySelector(`[data-midi="${midi}"] .thumb`)
      if (!thumb) continue

      if (usePitchClassMode) {
        // In pitch class mode, highlight if ANY note of this pitch class is active
        if (isPitchClassActive(midi % 12)) thumb.classList.add('midi-hit')
        else thumb.classList.remove('midi-hit')
      } else {
        if (midiActiveNotesRef.current.has(midi)) thumb.classList.add('midi-hit')
        else thumb.classList.remove('midi-hit')
      }
    }
  }, [startMidi, visibleCount])

  // ── Sync scroll position to C++ (debounced) ────────────────────────────
  const startMidiSyncTimer = useRef<ReturnType<typeof setTimeout>>()
  useEffect(() => {
    clearTimeout(startMidiSyncTimer.current)
    startMidiSyncTimer.current = setTimeout(() => {
      bridge.setStartMidi(startMidi)
    }, 500)
    return () => clearTimeout(startMidiSyncTimer.current)
  }, [startMidi, bridge])

  // ── Refine tonic MIDI when maqam list loads (for session recall) ──────
  useEffect(() => {
    if (!selectedMaqamId || maqamList.length === 0) return
    const entry = maqamList.find(m => m.maqamId === selectedMaqamId)
    if (!entry) return
    const tonicDisplay = selectedTransIdx >= 0 && entry.transpositions[selectedTransIdx]
      ? entry.transpositions[selectedTransIdx].tonicDisplay
      : entry.tonicDisplay
    const tonic = findTonicMidi(tonicDisplay, tuningState.noteNames)
    if (tonic) {
      setMaqamTonicIndex(tonic.chromaticIndex)
      setMaqamTonicMidi(tonic.midi)
    }
  }, [selectedMaqamId, selectedTransIdx, maqamList, tuningState.noteNames])

  // ── Re-apply or deactivate preset after system/note switch ──────────────
  // When maqam list changes (new system/note loaded), check if the pre-switch
  // preset is still valid. Uses ref because activePresetIndex may already be
  // cleared to -1 by handleSystemSelect before this effect runs.
  useEffect(() => {
    const presetToCheck = preSystemSwitchPresetRef.current
    if (presetToCheck < 0 || maqamList.length === 0) return
    preSystemSwitchPresetRef.current = -1  // consume
    systemSwitchPendingRef.current = false  // allow syncMaqamStateFromCpp to set preset again
    const preset = tuningState.presets[presetToCheck]
    if (!preset?.isAssigned) return
    const entry = maqamList.find(m => m.maqamId === preset.maqamId)
    const isCompatible = entry && !(preset.isTransposed && preset.setIndex >= 0 && preset.setIndex >= entry.transpositions.length)
    if (!isCompatible) {
      setActivePresetIndex(-1)
    } else {
      // Preset still valid in new system/note — re-apply so sliders update
      handlePresetClick(presetToCheck)
    }
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [maqamList])

  useJuceEvent('tuningSystemsLoaded', onTuningSystemsLoaded)
  useJuceEvent('tuningStateChanged',  onTuningStateChanged)
  useJuceEvent('statusMessage',       onStatusMessage)
  useJuceEvent('midiActivity',        onMidiActivity)
  useJuceEvent('maqamListLoaded',     onMaqamListLoaded)
  useJuceEvent('mtsStatusChanged',   onMtsStatusChanged)
  useJuceEvent('slotCentsChanged',   onSlotCentsChanged)

  // ── Maqam degree + scroll helpers ───────────────────────────────────────

  /** Get ascending degree names for a maqam selection. */
  const getAscendingDegrees = useCallback((maqamId: string, transIdx: number): string[] => {
    const entry = maqamList.find(m => m.maqamId === maqamId)
    if (!entry) return []
    if (transIdx >= 0 && transIdx < entry.transpositions.length)
      return entry.transpositions[transIdx].degrees.ascending
    return entry.degrees.ascending
  }, [maqamList])

  /** Get tonic PAO id + display name for a maqam selection. */
  const getTonicInfo = useCallback((maqamId: string, transIdx: number): { id: string; display: string } | null => {
    const entry = maqamList.find(m => m.maqamId === maqamId)
    if (!entry) return null
    if (transIdx >= 0 && transIdx < entry.transpositions.length)
      return { id: entry.transpositions[transIdx].tonicId, display: entry.transpositions[transIdx].tonicDisplay }
    return { id: entry.tonicId, display: entry.tonicDisplay }
  }, [maqamList])

  // ── Handlers ──────────────────────────────────────────────────────────────
  const handleSystemSelect = async (systemId: string, startingNote: string) => {
    showStatus('Loading ' + systemId + '…', 10000)
    // Capture active preset before clearing — used by maqam list useEffect to
    // re-apply or deactivate the preset after the new maqam list arrives.
    // systemSwitchPendingRef blocks syncMaqamStateFromCpp from restoring the
    // old C++ preset index before the useEffect can check compatibility.
    preSystemSwitchPresetRef.current = activePresetIndex
    systemSwitchPendingRef.current = true
    // Clear maqam state when switching systems
    setMaqamList([])
    setSelectedMaqamId('')
    setSelectedTransIdx(-1)
    setActivePresetIndex(-1)
    setMaqamDegreeIndices(EMPTY_SET)
    setMaqamTonicIndex(-1)
    setMaqamTonicMidi(-1)
    setIsMaqamModified(false)
    isMaqamModifiedRef.current = false  // Update ref immediately
    setModifiedSlots(new Set())
    setMaqamDegreePaoNames(new Map())
    maqamListRequested.current = false
    // Center on C3 (no maqam selected) and reset sticky viewport
    stickyViewportRef.current = false
    setStartMidi(centerMaqamOctave(48, fractionalVisibleCount))
    await bridge.selectTuningSystem(systemId, startingNote)
  }

  /** Snap marker click: select a specific variant (exact tuning system value). */
  const handleVariantSelect = useCallback(async (
    chromaticIndex: number,
    variantIndex: number
  ) => {
    const newState = await bridge.setSliderVariant(chromaticIndex, variantIndex)
    if (newState) setTuningState(newState)
    presetIndexOverrideRef.current = null
    setActivePresetIndex(-1)

    if (!selectedMaqamId) return

    // Check if this variant's PAO name matches the maqam's expected degree for this slot.
    // PAO names are the constant across tuning systems — variant indices can differ.
    const expectedPaoName = maqamDegreePaoNames.get(chromaticIndex)
    if (expectedPaoName === undefined) {
      // Non-degree slot — check if variant is the default (closest to 0 cents)
      const slot = newState?.slots[chromaticIndex]
      if (slot) {
        let defaultIdx = 0
        let minAbs = Infinity
        for (let i = 0; i < slot.variants.length; i++) {
          const abs = Math.abs(slot.variants[i].midiCentsDeviation)
          if (abs < minAbs) { minAbs = abs; defaultIdx = i }
        }
        if (variantIndex === defaultIdx) {
          // Back to default — remove from modified set
          setModifiedSlots(prev => {
            const next = new Set(prev)
            next.delete(chromaticIndex)
            return next
          })
          return
        }
      }
      setModifiedSlots(prev => new Set(prev).add(chromaticIndex))
      return
    }
    const selectedVariant = newState?.slots[chromaticIndex]?.variants[variantIndex]
    const selectedPaoName = selectedVariant?.noteName
    const isCorrectDegree = selectedPaoName === expectedPaoName

    if (isCorrectDegree) {
      // Correct PAO name for this maqam degree → remove from modified set
      setModifiedSlots(prev => {
        const next = new Set(prev)
        next.delete(chromaticIndex)
        if (next.size === 0) {
          setIsMaqamModified(false)
          isMaqamModifiedRef.current = false
        }
        return next
      })
    } else {
      // Different PAO name than maqam expects → mark as modified
      setIsMaqamModified(true)
      isMaqamModifiedRef.current = true
      setModifiedSlots(prev => new Set(prev).add(chromaticIndex))
    }
  }, [bridge, maqamDegreePaoNames, selectedMaqamId])

  /** Continuous drag: fire-and-forget update to C++ (MTS-ESP updates live).
   *  Uses requestAnimationFrame to throttle C++ calls to ~60fps. */
  const pendingDragRef = useRef<{ ci: number; cents: number } | null>(null)
  const dragRafRef = useRef<number>(0)
  const maqamModifiedRef = useRef(false)

  // Keep refs in sync with state during render (backup for when handlers don't update them)
  maqamDegreeIndicesRef.current = maqamDegreeIndices
  selectedMaqamIdRef.current = selectedMaqamId
  isMaqamModifiedRef.current = isMaqamModified

  const handleCentsDrag = useCallback((
    chromaticIndex: number,
    centsValue: number
  ) => {
    // Optimistic local state update for responsive UI
    setTuningState(prev => {
      const slots = [...prev.slots]
      slots[chromaticIndex] = { ...slots[chromaticIndex], centsOffset: centsValue }
      return { ...prev, slots }
    })

    // Track modifications for all slots when a maqam is selected
    // Use refs to always access latest values without stale closures
    const isDegreeSlot = maqamDegreeIndicesRef.current.has(chromaticIndex)
    const currentMaqamId = selectedMaqamIdRef.current
    if (currentMaqamId) {
      // Mark maqam as modified once per drag gesture (keep degrees highlighted)
      if (isDegreeSlot && !maqamModifiedRef.current) {
        maqamModifiedRef.current = true
        presetIndexOverrideRef.current = null
        setActivePresetIndex(-1)
        setIsMaqamModified(true)
        isMaqamModifiedRef.current = true  // Update ref immediately
      }
      // Track this specific slot as modified
      setModifiedSlots(prev => {
        if (prev.has(chromaticIndex)) return prev
        return new Set(prev).add(chromaticIndex)
      })
    }

    // Throttle C++ calls to one per animation frame
    pendingDragRef.current = { ci: chromaticIndex, cents: centsValue }
    if (!dragRafRef.current) {
      dragRafRef.current = requestAnimationFrame(() => {
        dragRafRef.current = 0
        if (pendingDragRef.current) {
          bridge.setSlotCents(pendingDragRef.current.ci, pendingDragRef.current.cents)
          pendingDragRef.current = null
        }
      })
    }
  }, [bridge])

  /** Drag end: finalize + get full state sync from C++. */
  const handleCentsDragEnd = useCallback(async (
    chromaticIndex: number,
    centsValue: number
  ) => {
    // Cancel any pending RAF
    if (dragRafRef.current) {
      cancelAnimationFrame(dragRafRef.current)
      dragRafRef.current = 0
    }
    pendingDragRef.current = null
    maqamModifiedRef.current = false

    const newState = await bridge.setSlotCentsFinalize(chromaticIndex, centsValue)
    if (newState) setTuningState(newState)
  }, [bridge])

  /** Gesture start: notify DAW of parameter change beginning (for automation recording). */
  const handleGestureStart = useCallback((chromaticIndex: number) => {
    bridge.beginSliderGesture(chromaticIndex)
  }, [bridge])

  /** Gesture end: notify DAW of parameter change ending (for automation recording). */
  const handleGestureEnd = useCallback((chromaticIndex: number) => {
    bridge.endSliderGesture(chromaticIndex)
  }, [bridge])

  // ── Reference frequency handlers ────────────────────────────────────────
  const pendingRefFreqRef = useRef<number>(0)
  const refFreqRafRef = useRef<number>(0)

  const handleRefFreqDrag = useCallback((cents: number) => {
    // Optimistic local state update
    const defaultHz = tuningState.referenceDefaultHz || 440
    setTuningState(prev => ({
      ...prev,
      referenceFreqCents: cents,
      referenceFreqHz: defaultHz * Math.pow(2, cents / 1200),
    }))
    // RAF-throttled bridge call
    pendingRefFreqRef.current = cents
    if (!refFreqRafRef.current) {
      refFreqRafRef.current = requestAnimationFrame(() => {
        refFreqRafRef.current = 0
        bridge.setReferenceFreqCents(pendingRefFreqRef.current)
      })
    }
  }, [bridge, tuningState.referenceDefaultHz])

  const handleRefFreqDragEnd = useCallback(async (cents: number) => {
    if (refFreqRafRef.current) {
      cancelAnimationFrame(refFreqRafRef.current)
      refFreqRafRef.current = 0
    }
    const newState = await bridge.setReferenceFreqCentsFinalize(cents)
    if (newState) setTuningState(newState)
  }, [bridge])

  const handleRefFreqGestureStart = useCallback(() => {
    bridge.beginRefFreqGesture()
  }, [bridge])

  const handleRefFreqGestureEnd = useCallback(() => {
    bridge.endRefFreqGesture()
  }, [bridge])

  const handleOscillatorToggle = useCallback(() => {
    const newEnabled = !tuningState.oscillatorEnabled
    setTuningState(prev => ({ ...prev, oscillatorEnabled: newEnabled }))
    bridge.setOscillatorEnabled(newEnabled)
  }, [bridge, tuningState.oscillatorEnabled])

  const handleHeptToggle = useCallback(() => {
    const newEnabled = !tuningState.heptEnabled
    setTuningState(prev => ({ ...prev, heptEnabled: newEnabled }))
    bridge.setHeptEnabled(newEnabled)
  }, [bridge, tuningState.heptEnabled])

  const handleMaqamSelect = async (maqamId: string, transpositionIndex: number) => {
    setSelectedMaqamId(maqamId)
    setSelectedTransIdx(transpositionIndex)
    setIsMaqamModified(false)  // Reset modification flag when selecting a new maqam
    isMaqamModifiedRef.current = false  // Update ref immediately
    setModifiedSlots(new Set())
    // Update ref immediately so modification tracking works even before React re-renders
    selectedMaqamIdRef.current = maqamId

    // Auto-activate matching unmodified preset (exact maqam + tonic, not modified)
    const matchingPreset = tuningState.presets.findIndex(p =>
      p.isAssigned && p.maqamId === maqamId && p.setIndex === transpositionIndex && !p.tuningSystemId
    )
    setActivePresetIndex(matchingPreset)
    // Guard against async tuningStateChanged event overwriting this value
    presetIndexOverrideRef.current = matchingPreset >= 0 ? matchingPreset : null

    const newState = await bridge.applyMaqam(maqamId, transpositionIndex)
    if (newState) {
      setTuningState(newState)
      // Compute maqam degree highlights and PAO name map using paoNameMap (covers all octaves)
      const degrees = getAscendingDegrees(maqamId, transpositionIndex)
      const degreeIndices = computeMaqamDegreeIndices(degrees, newState.paoNameMap)
      setMaqamDegreeIndices(degreeIndices)
      maqamDegreeIndicesRef.current = degreeIndices  // Update ref immediately
      setMaqamDegreePaoNames(buildDegreePaoNameMap(degrees, newState.paoNameMap))
      // Set tonic index + scroll so maqam octave is centered in viewport
      const tonicInfo = getTonicInfo(maqamId, transpositionIndex)
      if (tonicInfo) {
        const tonic = findTonicMidi(tonicInfo.display, newState.noteNames)
        if (tonic) {
          setMaqamTonicIndex(tonic.chromaticIndex)
          setMaqamTonicMidi(tonic.midi)
          if (!stickyViewportRef.current)
            setStartMidi(centerMaqamOctave(tonic.midi, fractionalVisibleCount))
        }
      }
    }
    // Re-assert preset match after state processing (in case async event arrived during await)
    if (matchingPreset >= 0) setActivePresetIndex(matchingPreset)
  }

  const handleSaveToPreset = async (presetIndex: number) => {
    if (!selectedMaqamId) {
      showStatus('Select a maqam first')
      return
    }
    const entry = maqamList.find(m => m.maqamId === selectedMaqamId)
    if (!entry) return

    const positions = tuningState.slots.map(s => s.selectedIndex)
    const centsOffsets = tuningState.slots.map(s => s.centsOffset)

    // Resolve tonic display name, IPN, and solfege for the selected transposition
    const tonicId = selectedTransIdx >= 0 && entry.transpositions[selectedTransIdx]
      ? entry.transpositions[selectedTransIdx].tonicId
      : entry.tonicId
    const tonicDisplay = selectedTransIdx >= 0 && entry.transpositions[selectedTransIdx]
      ? entry.transpositions[selectedTransIdx].tonicDisplay
      : entry.tonicDisplay
    const tonicMidi = findTonicMidi(tonicDisplay, tuningState.noteNames)
    const IPN_NAMES = ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']
    const tonicIpnLabel = tonicMidi
      ? `${IPN_NAMES[tonicMidi.chromaticIndex]}${Math.floor(tonicMidi.midi / 12) - 1}`
      : ''
    const tonicSolfege = tuningState.paoNameInfo[tonicId]?.solfege || ''

    const degreeNames = getAscendingDegrees(selectedMaqamId, selectedTransIdx)

    // Include asterisk in display name if maqam tuning was modified
    const displayName = isMaqamModified ? entry.maqamDisplay + ' *' : entry.maqamDisplay

    // Store tuning system info for modified presets (needed to switch systems when loading)
    const tuningSystemId = isMaqamModified ? tuningState.systemId : ''
    const startingNote = isMaqamModified ? tuningState.startingNote : ''

    await bridge.assignPreset(
      presetIndex, entry.maqamId, displayName,
      entry.familyId, selectedTransIdx >= 0, tonicDisplay, tonicIpnLabel, tonicSolfege,
      selectedTransIdx, positions, degreeNames, centsOffsets, tuningSystemId, startingNote
    )
    // Update local preset state immediately
    setTuningState(prev => {
      const presets = [...prev.presets]
      presets[presetIndex] = {
        isAssigned: true,
        maqamId: entry.maqamId,
        maqamDisplay: displayName,
        tonicIpn: tonicIpnLabel,
        tonicSolfege,
        baseMaqamId: entry.familyId,
        isTransposed: selectedTransIdx >= 0,
        tonicNote: tonicDisplay,
        setIndex: selectedTransIdx,
        sliderPositions: positions,
        centsOffsets,
        degreeNames,
        tuningSystemId,
        startingNote,
      }
      return { ...prev, presets }
    })
    setActivePresetIndex(presetIndex)
  }

  /** Apply a modified preset with modification tracking. */
  const applyModifiedPreset = async (presetIndex: number) => {
    const preset = tuningState.presets[presetIndex]
    if (!preset?.isAssigned) return

    // For modified presets, applyPreset handles everything:
    // - applies slider positions and centsOffsets from preset
    // - restores maqam state (currentMaqamId, degreeNames, etc.)
    // No need for applyMaqam (which would fail anyway if maqam list isn't loaded yet)
    setActivePresetIndex(presetIndex)
    // Guard against async tuningStateChanged (e.g. fetchAndApplyMaqamDetail) overwriting preset
    presetIndexOverrideRef.current = presetIndex
    const newState = await bridge.applyPreset(presetIndex)
    if (newState) setTuningState(newState)

    // Build PAO name map and track modifications
    const degrees = preset.degreeNames?.length > 0
      ? preset.degreeNames
      : getAscendingDegrees(preset.maqamId, preset.setIndex)
    const degreePaoNames = newState
      ? buildDegreePaoNameMap(degrees, newState.paoNameMap)
      : new Map<number, string>()
    setMaqamDegreePaoNames(degreePaoNames)

    // Compare preset's cents values to current tuning system's defaults
    const modified = new Set<number>()
    if (newState) {
      for (let i = 0; i < 12; i++) {
        const expectedPaoName = degreePaoNames.get(i)
        if (expectedPaoName === undefined) continue

        const slot = newState.slots[i]
        const expectedVariant = slot.variants.find(v => v.noteName === expectedPaoName)
        if (!expectedVariant) continue

        const systemDefault = expectedVariant.midiCentsDeviation
        if (Math.abs(slot.centsOffset - systemDefault) > 0.01) {
          modified.add(i)
        }
      }
    }
    setModifiedSlots(modified)
    setIsMaqamModified(modified.size > 0)

    // Sync UI state
    setSelectedMaqamId(preset.maqamId)
    setSelectedTransIdx(preset.setIndex)
    if (newState) {
      setMaqamDegreeIndices(computeMaqamDegreeIndices(degrees, newState.paoNameMap))
      const tonicInfo = getTonicInfo(preset.maqamId, preset.setIndex)
      if (tonicInfo) {
        const tonic = findTonicMidi(tonicInfo.display, newState.noteNames)
        if (tonic) {
          setMaqamTonicIndex(tonic.chromaticIndex)
          setMaqamTonicMidi(tonic.midi)
          if (!stickyViewportRef.current)
            setStartMidi(centerMaqamOctave(tonic.midi, fractionalVisibleCount))
        }
      }
    }
  }

  /** Apply an unmodified preset — just applies the maqam fresh in current system. */
  const applyUnmodifiedPreset = async (presetIndex: number) => {
    const preset = tuningState.presets[presetIndex]
    if (!preset?.isAssigned) return

    // Just apply the maqam (use current system's cents, not preset's stored values)
    setActivePresetIndex(presetIndex)
    // Guard against async tuningStateChanged (e.g. fetchAndApplyMaqamDetail) overwriting preset
    presetIndexOverrideRef.current = presetIndex
    const newState = await bridge.applyMaqam(preset.maqamId, preset.setIndex)
    if (newState) setTuningState(newState)

    // No modifications — it's the canonical maqam
    setIsMaqamModified(false)
    isMaqamModifiedRef.current = false
    setModifiedSlots(new Set())

    // Build PAO name map for future modification tracking
    const degrees = preset.degreeNames?.length > 0
      ? preset.degreeNames
      : getAscendingDegrees(preset.maqamId, preset.setIndex)
    const degreePaoNames = newState
      ? buildDegreePaoNameMap(degrees, newState.paoNameMap)
      : new Map<number, string>()
    setMaqamDegreePaoNames(degreePaoNames)

    // Sync UI state
    setSelectedMaqamId(preset.maqamId)
    setSelectedTransIdx(preset.setIndex)
    if (newState) {
      setMaqamDegreeIndices(computeMaqamDegreeIndices(degrees, newState.paoNameMap))
      const tonicInfo = getTonicInfo(preset.maqamId, preset.setIndex)
      if (tonicInfo) {
        const tonic = findTonicMidi(tonicInfo.display, newState.noteNames)
        if (tonic) {
          setMaqamTonicIndex(tonic.chromaticIndex)
          setMaqamTonicMidi(tonic.midi)
          if (!stickyViewportRef.current)
            setStartMidi(centerMaqamOctave(tonic.midi, fractionalVisibleCount))
        }
      }
    }
  }

  const handlePresetClick = async (presetIndex: number) => {
    // -1 = deactivate current preset (clear maqam, revert to system-only state)
    if (presetIndex === -1) {
      presetIndexOverrideRef.current = null
      setSelectedMaqamId('')
      selectedMaqamIdRef.current = ''
      setSelectedTransIdx(-1)
      setActivePresetIndex(-1)
      setMaqamDegreeIndices(EMPTY_SET)
      maqamDegreeIndicesRef.current = EMPTY_SET
      setMaqamTonicIndex(-1)
      setMaqamTonicMidi(-1)
      setIsMaqamModified(false)
      isMaqamModifiedRef.current = false
      setModifiedSlots(new Set())
      setMaqamDegreePaoNames(new Map())
      // Keep viewport where it is — don't scroll on preset deactivation
      stickyViewportRef.current = true
      // Re-select the current tuning system to reset C++ sliders to base values
      await bridge.selectTuningSystem(tuningState.systemId, tuningState.startingNote)
      return
    }

    const preset = tuningState.presets[presetIndex]
    if (!preset?.isAssigned) return

    // Check if preset is modified (has tuningSystemId stored)
    const isModifiedPreset = !!preset.tuningSystemId

    if (!isModifiedPreset) {
      // Unmodified preset: just apply the maqam in current system
      await applyUnmodifiedPreset(presetIndex)
      return
    }

    // Modified preset: need to be in the correct tuning system
    const needsSystemSwitch =
      preset.tuningSystemId !== tuningState.systemId ||
      preset.startingNote !== tuningState.startingNote

    if (needsSystemSwitch) {
      // Switch tuning systems first, then apply preset after load
      showStatus('Loading ' + preset.tuningSystemId + '…', 10000)
      afterSystemSwitchRef.current = () => { applyModifiedPreset(presetIndex) }
      await bridge.selectTuningSystem(preset.tuningSystemId, preset.startingNote)
    } else {
      // Already in correct system — apply directly
      await applyModifiedPreset(presetIndex)
    }
  }

  const handlePresetClear = async (presetIndex: number) => {
    await bridge.clearPreset(presetIndex)
    setTuningState(prev => {
      const presets = [...prev.presets]
      presets[presetIndex] = {
        isAssigned: false, maqamId: '', maqamDisplay: '', tonicIpn: '', tonicSolfege: '',
        baseMaqamId: '', isTransposed: false, tonicNote: '', setIndex: -1, sliderPositions: [],
        centsOffsets: [],
        degreeNames: [],
        tuningSystemId: '',
        startingNote: '',
      }
      return { ...prev, presets }
    })
    if (activePresetIndex === presetIndex) {
      setActivePresetIndex(-1)
      setMaqamDegreeIndices(EMPTY_SET)
      setMaqamTonicIndex(-1)
      setMaqamTonicMidi(-1)
    }
  }

  const handleCheckForUpdates = async () => {
    showStatus('Checking for updates…', 10000)
    const updated = await bridge.checkForUpdates()
    showStatus(updated.length > 0
      ? `Updated: ${updated.join(', ')}`
      : 'Data is up to date')
  }

  // ── MIDI Learn handlers ─────────────────────────────────────────────────
  const handleMidiLearnStart = useCallback((presetIndex: number) => {
    bridge.startMidiLearn(presetIndex)
    showStatus(`MIDI Learn: Press a note to map to preset ${presetIndex + 1}`, 10000)
  }, [bridge, showStatus])

  const handleMidiLearnCancel = useCallback(() => {
    bridge.cancelMidiLearn()
    showStatus('MIDI Learn cancelled')
  }, [bridge, showStatus])

  const handleMidiNoteClear = useCallback((presetIndex: number) => {
    bridge.clearMidiPresetNote(presetIndex)
    showStatus(`Cleared MIDI mapping for preset ${presetIndex + 1}`)
  }, [bridge, showStatus])

  const handleSaveState = useCallback(async () => {
    const filename = await bridge.saveStateFile()
    if (filename) showStatus(`Saved: ${filename}`)
  }, [bridge, showStatus])

  const handleLoadState = useCallback(async () => {
    // Set up callback to compute modification state after the loaded tuning system arrives.
    // loadTuningSystem fires notifyTuningChanged() multiple times: intermediate events with
    // cleared maqam state, then the final event from restoreStateFromJson's completion lambda
    // with restored maqam ID and degree names. Previous pending tuningStateChanged events
    // (from slider drags, etc.) may also arrive before the load events.
    //
    // Strategy: skip ALL events without maqam data, process the first with maqam data.
    // For files without maqam, we consume the callback after the bridge call returns.
    afterSystemSwitchRef.current = (newState: TuningState): void | false => {
      const degreeNames = newState.degreeNames ?? []
      const maqamId = newState.selectedMaqamId ?? ''

      // Wait for the event with restored maqam state
      if (!maqamId || degreeNames.length === 0) {
        return false  // keep callback — this is an intermediate or stale event
      }

      const degreePaoNames = buildDegreePaoNameMap(degreeNames, newState.paoNameMap)
      setMaqamDegreePaoNames(degreePaoNames)

      // Compare loaded cents values to tuning system defaults to detect modifications
      const modified = new Set<number>()
      for (let i = 0; i < 12; i++) {
        const slot = newState.slots[i]
        const expectedPaoName = degreePaoNames.get(i)
        if (expectedPaoName !== undefined) {
          // Maqam degree — compare to expected variant
          const expectedVariant = slot.variants.find(v => v.noteName === expectedPaoName)
          if (!expectedVariant) continue
          if (Math.abs(slot.centsOffset - expectedVariant.midiCentsDeviation) > 0.01)
            modified.add(i)
        } else {
          // Non-degree — compare to default variant (closest to 0 cents)
          let defaultCents = 0
          let minAbs = Infinity
          for (const v of slot.variants) {
            const abs = Math.abs(v.midiCentsDeviation)
            if (abs < minAbs) { minAbs = abs; defaultCents = v.midiCentsDeviation }
          }
          if (Math.abs(slot.centsOffset - defaultCents) > 0.01)
            modified.add(i)
        }
      }
      setModifiedSlots(modified)
      setIsMaqamModified(modified.size > 0)
      isMaqamModifiedRef.current = modified.size > 0

      // Sync degree indices and tonic
      const degreeIndices = computeMaqamDegreeIndices(degreeNames, newState.paoNameMap)
      setMaqamDegreeIndices(degreeIndices)
      maqamDegreeIndicesRef.current = degreeIndices
      setSelectedMaqamId(maqamId)
      selectedMaqamIdRef.current = maqamId
      setSelectedTransIdx(newState.transpositionIndex ?? -1)
      setActivePresetIndex(newState.activePresetIndex ?? -1)

      const tonicName = degreeNames[0]
      const tonicCi = newState.paoNameMap[tonicName]
      if (tonicCi !== undefined) {
        setMaqamTonicIndex(tonicCi)
        const tonic = findTonicMidi(
          Object.entries(newState.noteNames[String(tonicCi)] || {}).find(([, name]) =>
            newState.paoNameMap[name] === tonicCi
          )?.[1] || tonicName,
          newState.noteNames
        )
        const tonicMidi = tonic?.midi ?? (tonicCi + 48)
        setMaqamTonicMidi(tonicMidi)
        setStartMidi(centerMaqamOctave(tonicMidi, fractionalVisibleCount))
      }
    }

    const result = await bridge.loadStateFile()
    if (!result) {
      // Cancelled — clear the callback
      afterSystemSwitchRef.current = null
      return
    }

    // For files without maqam data, the callback will never see an event with maqam state.
    // Consume it now and clear modification state.
    if (!result.hasMaqam && afterSystemSwitchRef.current) {
      afterSystemSwitchRef.current = null
      setIsMaqamModified(false)
      isMaqamModifiedRef.current = false
      setModifiedSlots(new Set())
      setMaqamDegreePaoNames(new Map())
    }

    showStatus(`Loaded: ${result.filename}`)
  }, [bridge, showStatus, fractionalVisibleCount])

  return (
    <div className="app">
      <div className="top-bar">
        <div className="top-bar-left">
          <div className="menu-bar">
            <span className="menu-bar-item" onClick={handleLoadState}>Load</span>
            <span className="menu-bar-item" onClick={handleSaveState}>Save</span>
            <span className="menu-bar-item">Settings</span>
          </div>
          <TuningSystemSelector
            systems={tuningSystems}
            currentSystemId={tuningState.systemId}
            currentStartingNote={tuningState.startingNote}
            onSelect={handleSystemSelect}
          />
          <MaqamSelector
            maqamList={maqamList}
            selectedMaqamId={selectedMaqamId}
            selectedTranspositionIndex={selectedTransIdx}
            paoOrder={tuningState.paoOrder}
            paoNameInfo={tuningState.paoNameInfo}
            isModified={isMaqamModified}
            activePresetIndex={activePresetIndex}
            onSelect={handleMaqamSelect}
          />
        </div>
        <div className="top-bar-right">
          <ReferenceFreqControl
            cents={tuningState.referenceFreqCents}
            hz={tuningState.referenceFreqHz}
            defaultHz={tuningState.referenceDefaultHz}
            tonicChromaticIndex={maqamTonicIndex >= 0
              ? maqamTonicIndex
              : (findTonicMidi(tuningState.referenceNoteName, tuningState.noteNames)?.chromaticIndex ?? -1)}
            ipnNames={tuningState.slots.map((slot, i) =>
              tuningState.degreeIpnMap[String(i)] || slot.ipnRef
            )}
            onDrag={handleRefFreqDrag}
            onDragEnd={handleRefFreqDragEnd}
            onGestureStart={handleRefFreqGestureStart}
            onGestureEnd={handleRefFreqGestureEnd}
          />
          <OutputModeSelector
            isMtsTransmitter={tuningState.isMtsTransmitter}
            oscillatorEnabled={tuningState.oscillatorEnabled}
            heptEnabled={tuningState.heptEnabled}
            hasMaqam={selectedMaqamId !== ''}
            mtsNativeCount={tuningState.mtsNativeCount}
            mpeCount={tuningState.mpeCount}
            monoPbCount={tuningState.monoPbCount}
            onOscillatorToggle={handleOscillatorToggle}
            onHeptToggle={handleHeptToggle}
          />
        </div>
      </div>

      <MaqamPresetBar
        presets={tuningState.presets}
        activePresetIndex={activePresetIndex}
        maqamList={maqamList}
        midiLearnTarget={tuningState.midiLearnTarget ?? -1}
        midiPresetNotes={tuningState.midiPresetNotes ?? Array(16).fill(-1)}
        onPresetClick={handlePresetClick}
        onSaveToPreset={handleSaveToPreset}
        onPresetClear={handlePresetClear}
        onMidiLearnStart={handleMidiLearnStart}
        onMidiLearnCancel={handleMidiLearnCancel}
        onMidiNoteClear={handleMidiNoteClear}
      />

      <RangeScroller startMidi={startMidi} visibleCount={visibleCount} maqamTonicMidi={maqamTonicMidi} onChange={handleRangeScrollerChange} />

      <div className="slider-bank-container" ref={bankContainerRef}>
        <NoteSliderBank
          slots={tuningState.slots}
          startMidi={startMidi}
          bankWidthPx={bankWidthPx}
          noteNames={tuningState.noteNames}
          perNoteOverrides={tuningState.perNoteOverrides}
          degreeIpnMap={tuningState.degreeIpnMap}
          degreeSolfegeMap={tuningState.degreeSolfegeMap}
          maqamDegreeIndices={maqamDegreeIndices}
          maqamTonicIndex={maqamTonicIndex}
          maqamTonicMidi={maqamTonicMidi}
          modifiedSlots={modifiedSlots}
          maqamDegreePaoNames={maqamDegreePaoNames}
          heptEnabled={tuningState.heptEnabled && maqamDegreeIndices.size > 0}
          onVariantSelect={handleVariantSelect}
          onCentsDrag={handleCentsDrag}
          onCentsDragEnd={handleCentsDragEnd}
          onGestureStart={handleGestureStart}
          onGestureEnd={handleGestureEnd}
        />
      </div>

      <StatusBar
        status={status}
        pluginVersion={tuningState.pluginVersion}
        buildTimestamp={tuningState.buildTimestamp}
        onCheckForUpdates={handleCheckForUpdates}
      />
    </div>
  )
}
