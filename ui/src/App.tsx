import { useState, useEffect, useCallback, useRef } from 'react'
import type { TuningState, TuningSystem, MaqamListEntry, MtsStatusUpdate } from './types'
import { useJuceBridge, useJuceEvent } from './hooks/useJuceBridge'
import { useVisibleSliderCount } from './hooks/useVisibleSliderCount'
import { SLOT_WIDTH_PX } from './constants'
import './App.css'

import TuningSystemSelector from './components/TuningSystemSelector'
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
  mtsReceivers: 0,
  mtsNativeCount: 0,
  mpeCount: 0,
  monoPbCount: 0,
  pluginVersion: '',
  slots: Array.from({ length: 12 }, (_, i) => ({
    ipnRef: ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B'][i],
    selectedIndex: 0,
    isLocked: true,
    variants: [],
    centsOffset: 0,
  })),
  presets: Array.from({ length: 16 }, () => ({
    isAssigned: false, maqamId: '', maqamDisplay: '', tonicIpn: '',
    baseMaqamId: '', isTransposed: false, tonicNote: '', setIndex: -1, sliderPositions: [],
    centsOffsets: [],
    degreeNames: [],
  })),
  noteNames: {},
  perNoteOverrides: {},
  paoNameMap: {},
  paoOrder: [],
  paoNameInfo: {},
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

/** Center a maqam's octave (12 notes) within the visible slider viewport.
 *  Uses fractional visibleCount for smooth "curtain opening" effect. */
function centerMaqamOctave(tonicMidi: number, visibleCount: number): number {
  const padding = (visibleCount - 12) / 2
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
  const [maqamDefaultPositions, setMaqamDefaultPositions] = useState<number[]>([])  // original variant indices when maqam was applied
  const maqamListRequested = useRef(false)

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

  /** Sync JS maqam state from C++ tuning state (used on session recall). */
  const syncMaqamStateFromCpp = useCallback((state: TuningState) => {
    setSelectedMaqamId(state.selectedMaqamId || '')
    setSelectedTransIdx(state.transpositionIndex ?? -1)
    setActivePresetIndex(state.activePresetIndex ?? -1)
    if (state.startMidi !== undefined) setStartMidi(state.startMidi)

    if (state.degreeNames && state.degreeNames.length > 0) {
      setMaqamDegreeIndices(computeMaqamDegreeIndices(state.degreeNames, state.paoNameMap))
      const tonicCi = state.paoNameMap[state.degreeNames[0]]
      if (tonicCi !== undefined) {
        setMaqamTonicIndex(tonicCi)
        setMaqamTonicMidi(tonicCi + 48) // base register approximation
      }
    } else {
      setMaqamDegreeIndices(EMPTY_SET)
      setMaqamTonicIndex(-1)
      setMaqamTonicMidi(-1)
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

    // Session recall completed — sync maqam state from C++
    if (state.hasRecalledSessionState && !prevRecalledRef.current) {
      prevRecalledRef.current = true
      syncMaqamStateFromCpp(state)
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
  const [isUserScrolling, setIsUserScrolling] = useState(false)
  const bankContainerRef = useRef<HTMLDivElement>(null)
  const { count: visibleCount, widthPx: bankWidthPx } = useVisibleSliderCount(bankContainerRef)
  const userScrollTimeoutRef = useRef<ReturnType<typeof setTimeout>>()

  // Fractional visible count for smooth curtain effect
  const fractionalVisibleCount = bankWidthPx / SLOT_WIDTH_PX

  // Keep maqam's octave (or default C3 octave) centered during resize (smooth curtain effect)
  // Only apply centering when there's clearly more than 12 sliders of space
  useEffect(() => {
    if (isUserScrolling) return // Don't override user's manual scroll position
    const tonicMidi = maqamTonicMidi >= 0 ? maqamTonicMidi : 48
    // At minimum width (~12 sliders), just start at tonic without centering
    // This avoids fractional calculations that show partial sliders
    if (fractionalVisibleCount < 12.5) {
      setStartMidi(tonicMidi)
    } else {
      setStartMidi(centerMaqamOctave(tonicMidi, fractionalVisibleCount))
    }
  }, [fractionalVisibleCount, maqamTonicMidi, isUserScrolling])

  const handleBankWheel = useCallback((e: React.WheelEvent) => {
    setIsUserScrolling(true)
    // Clear any existing timeout and set a new one
    if (userScrollTimeoutRef.current) clearTimeout(userScrollTimeoutRef.current)
    userScrollTimeoutRef.current = setTimeout(() => setIsUserScrolling(false), 2000)

    const delta = e.deltaY / SLOT_WIDTH_PX
    setStartMidi(prev => Math.max(0, Math.min(128 - fractionalVisibleCount, prev + delta)))
  }, [fractionalVisibleCount])

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

  useJuceEvent('tuningSystemsLoaded', onTuningSystemsLoaded)
  useJuceEvent('tuningStateChanged',  onTuningStateChanged)
  useJuceEvent('statusMessage',       onStatusMessage)
  useJuceEvent('midiActivity',        onMidiActivity)
  useJuceEvent('maqamListLoaded',     onMaqamListLoaded)
  useJuceEvent('mtsStatusChanged',   onMtsStatusChanged)

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
    // Clear maqam state when switching systems
    setMaqamList([])
    setSelectedMaqamId('')
    setSelectedTransIdx(-1)
    setActivePresetIndex(-1)
    setMaqamDegreeIndices(EMPTY_SET)
    setMaqamTonicIndex(-1)
    setMaqamTonicMidi(-1)
    setIsMaqamModified(false)
    setModifiedSlots(new Set())
    setMaqamDefaultPositions([])
    maqamListRequested.current = false
    await bridge.selectTuningSystem(systemId, startingNote)
  }

  /** Snap marker click: select a specific variant (exact tuning system value). */
  const handleVariantSelect = useCallback(async (
    chromaticIndex: number,
    variantIndex: number
  ) => {
    const newState = await bridge.setSliderVariant(chromaticIndex, variantIndex)
    if (newState) setTuningState(newState)
    setActivePresetIndex(-1)

    // Check if this variant matches the maqam's default for this slot
    const isDefaultVariant = maqamDefaultPositions.length > 0 &&
      maqamDefaultPositions[chromaticIndex] === variantIndex

    if (isDefaultVariant) {
      // Returning to maqam default → remove from modified set
      setModifiedSlots(prev => {
        const next = new Set(prev)
        next.delete(chromaticIndex)
        if (next.size === 0) setIsMaqamModified(false)
        return next
      })
    } else if (selectedMaqamId) {
      // Different variant than maqam default → mark as modified
      setIsMaqamModified(true)
      setModifiedSlots(prev => new Set(prev).add(chromaticIndex))
    }
  }, [bridge, maqamDefaultPositions, selectedMaqamId])

  /** Continuous drag: fire-and-forget update to C++ (MTS-ESP updates live).
   *  Uses requestAnimationFrame to throttle C++ calls to ~60fps. */
  const pendingDragRef = useRef<{ ci: number; cents: number } | null>(null)
  const dragRafRef = useRef<number>(0)
  const maqamModifiedRef = useRef(false)

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

    // Mark maqam as modified once per drag gesture (keep degrees highlighted)
    if (!maqamModifiedRef.current) {
      maqamModifiedRef.current = true
      setActivePresetIndex(-1)
      if (selectedMaqamId) setIsMaqamModified(true)
    }
    // Track this specific slot as modified
    if (selectedMaqamId) {
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
  }, [bridge, selectedMaqamId])

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

  const handleMaqamSelect = async (maqamId: string, transpositionIndex: number) => {
    setSelectedMaqamId(maqamId)
    setSelectedTransIdx(transpositionIndex)
    setActivePresetIndex(-1)
    setIsMaqamModified(false)  // Reset modification flag when selecting a new maqam
    setModifiedSlots(new Set())
    const newState = await bridge.applyMaqam(maqamId, transpositionIndex)
    if (newState) {
      setTuningState(newState)
      // Capture the maqam's default variant positions for modification tracking
      setMaqamDefaultPositions(newState.slots.map(s => s.selectedIndex))
      // Compute maqam degree highlights using paoNameMap (covers all octaves)
      const degrees = getAscendingDegrees(maqamId, transpositionIndex)
      setMaqamDegreeIndices(computeMaqamDegreeIndices(degrees, newState.paoNameMap))
      // Set tonic index + scroll so maqam octave is centered in viewport
      const tonicInfo = getTonicInfo(maqamId, transpositionIndex)
      if (tonicInfo) {
        const tonic = findTonicMidi(tonicInfo.display, newState.noteNames)
        if (tonic) {
          setMaqamTonicIndex(tonic.chromaticIndex)
          setMaqamTonicMidi(tonic.midi)
          setIsUserScrolling(false)
          setStartMidi(centerMaqamOctave(tonic.midi, fractionalVisibleCount))
        }
      }
    }
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

    // Resolve tonic display name and IPN for the selected transposition
    const tonicDisplay = selectedTransIdx >= 0 && entry.transpositions[selectedTransIdx]
      ? entry.transpositions[selectedTransIdx].tonicDisplay
      : entry.tonicDisplay
    const tonicMidi = findTonicMidi(tonicDisplay, tuningState.noteNames)
    const IPN_NAMES = ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']
    const tonicIpnLabel = tonicMidi
      ? `${IPN_NAMES[tonicMidi.chromaticIndex]}${Math.floor(tonicMidi.midi / 12) - 1}`
      : ''

    const degreeNames = getAscendingDegrees(selectedMaqamId, selectedTransIdx)

    // Include asterisk in display name if maqam tuning was modified
    const displayName = isMaqamModified ? entry.maqamDisplay + ' *' : entry.maqamDisplay

    await bridge.assignPreset(
      presetIndex, entry.maqamId, displayName,
      entry.familyId, selectedTransIdx >= 0, tonicDisplay, tonicIpnLabel,
      selectedTransIdx, positions, degreeNames, centsOffsets
    )
    // Update local preset state immediately
    setTuningState(prev => {
      const presets = [...prev.presets]
      presets[presetIndex] = {
        isAssigned: true,
        maqamId: entry.maqamId,
        maqamDisplay: displayName,
        tonicIpn: tonicIpnLabel,
        baseMaqamId: entry.familyId,
        isTransposed: selectedTransIdx >= 0,
        tonicNote: tonicDisplay,
        setIndex: selectedTransIdx,
        sliderPositions: positions,
        centsOffsets,
        degreeNames,
      }
      return { ...prev, presets }
    })
    setActivePresetIndex(presetIndex)
  }

  const handlePresetClick = async (presetIndex: number) => {
    // Get the preset to access its maqam info
    const preset = tuningState.presets[presetIndex]
    if (!preset?.isAssigned) return

    // First, apply the maqam to get its default positions (don't update UI state yet)
    const maqamState = await bridge.applyMaqam(preset.maqamId, preset.setIndex)
    const maqamDefaults = maqamState ? maqamState.slots.map(s => s.selectedIndex) : []
    setMaqamDefaultPositions(maqamDefaults)

    // Now apply the actual preset (this overwrites with saved positions/centsOffsets)
    const newState = await bridge.applyPreset(presetIndex)
    if (newState) setTuningState(newState)
    setActivePresetIndex(presetIndex)

    // Derive modified slots by comparing preset positions to maqam defaults
    const modified = new Set<number>()
    if (newState && maqamDefaults.length === 12) {
      for (let i = 0; i < 12; i++) {
        const slot = newState.slots[i]
        // Modified if: variant differs from maqam default OR centsOffset differs from variant's deviation
        const variant = slot.variants[slot.selectedIndex]
        const variantDiffersFromDefault = slot.selectedIndex !== maqamDefaults[i]
        const centsDiffersFromVariant = variant && Math.abs(slot.centsOffset - variant.midiCentsDeviation) > 0.01
        if (variantDiffersFromDefault || centsDiffersFromVariant) {
          modified.add(i)
        }
      }
    }
    setModifiedSlots(modified)
    setIsMaqamModified(modified.size > 0)

    // Sync dropdown selection to match preset's maqam
    if (preset.isAssigned) {
      setSelectedMaqamId(preset.maqamId)
      setSelectedTransIdx(preset.setIndex)
      // Recompute maqam degree highlights using paoNameMap (covers all octaves)
      if (newState) {
        const degrees = getAscendingDegrees(preset.maqamId, preset.setIndex)
        setMaqamDegreeIndices(computeMaqamDegreeIndices(degrees, newState.paoNameMap))
        // Set tonic index + scroll so maqam octave is centered in viewport
        const tonicInfo = getTonicInfo(preset.maqamId, preset.setIndex)
        if (tonicInfo) {
          const tonic = findTonicMidi(tonicInfo.display, newState.noteNames)
          if (tonic) {
            setMaqamTonicIndex(tonic.chromaticIndex)
            setMaqamTonicMidi(tonic.midi)
            setIsUserScrolling(false)
            setStartMidi(centerMaqamOctave(tonic.midi, fractionalVisibleCount))
          }
        }
      }
    }
  }

  const handlePresetClear = async (presetIndex: number) => {
    await bridge.clearPreset(presetIndex)
    setTuningState(prev => {
      const presets = [...prev.presets]
      presets[presetIndex] = {
        isAssigned: false, maqamId: '', maqamDisplay: '', tonicIpn: '',
        baseMaqamId: '', isTransposed: false, tonicNote: '', setIndex: -1, sliderPositions: [],
        centsOffsets: [],
        degreeNames: [],
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

  return (
    <div className="app">
      <div className="top-bar">
        <TuningSystemSelector
          systems={tuningSystems}
          currentSystemId={tuningState.systemId}
          currentStartingNote={tuningState.startingNote}
          onSelect={handleSystemSelect}
        />
        <OutputModeSelector
          isMtsTransmitter={tuningState.isMtsTransmitter}
          mtsNativeCount={tuningState.mtsNativeCount}
          mpeCount={tuningState.mpeCount}
          monoPbCount={tuningState.monoPbCount}
        />
      </div>

      <MaqamSelector
        maqamList={maqamList}
        selectedMaqamId={selectedMaqamId}
        selectedTranspositionIndex={selectedTransIdx}
        paoOrder={tuningState.paoOrder}
        paoNameInfo={tuningState.paoNameInfo}
        isModified={isMaqamModified}
        onSelect={handleMaqamSelect}
      />

      <MaqamPresetBar
        presets={tuningState.presets}
        activePresetIndex={activePresetIndex}
        maqamList={maqamList}
        onPresetClick={handlePresetClick}
        onSaveToPreset={handleSaveToPreset}
        onPresetClear={handlePresetClear}
      />

      <RangeScroller startMidi={startMidi} visibleCount={visibleCount} maqamTonicMidi={maqamTonicMidi} onChange={setStartMidi} />

      <div className="slider-bank-container" ref={bankContainerRef} onWheel={handleBankWheel}>
        <NoteSliderBank
          slots={tuningState.slots}
          startMidi={startMidi}
          bankWidthPx={bankWidthPx}
          noteNames={tuningState.noteNames}
          perNoteOverrides={tuningState.perNoteOverrides}
          maqamDegreeIndices={maqamDegreeIndices}
          maqamTonicIndex={maqamTonicIndex}
          maqamTonicMidi={maqamTonicMidi}
          modifiedSlots={modifiedSlots}
          onVariantSelect={handleVariantSelect}
          onCentsDrag={handleCentsDrag}
          onCentsDragEnd={handleCentsDragEnd}
        />
      </div>

      <StatusBar
        status={status}
        pluginVersion={tuningState.pluginVersion}
        onCheckForUpdates={handleCheckForUpdates}
      />
    </div>
  )
}
