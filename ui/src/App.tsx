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
  })),
  presets: Array.from({ length: 12 }, () => ({
    isAssigned: false, maqamId: '', maqamDisplay: '', tonicIpn: '',
    baseMaqamId: '', isTransposed: false, tonicNote: '', setIndex: -1, sliderPositions: [],
    degreeNames: [],
  })),
  noteNames: {},
  perNoteOverrides: {},
  paoNameMap: {},
  paoOrder: [],
  paoNameInfo: {},
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

/** Center a maqam's octave (12 notes) within the visible slider viewport. */
function centerMaqamOctave(tonicMidi: number, visibleCount: number): number {
  const padding = Math.floor((visibleCount - 12) / 2)
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
  const maqamListRequested = useRef(false)

  const showStatus = useCallback((msg: string, durationMs = 4000) => {
    setStatus(msg)
    clearTimeout(statusTimer.current)
    if (msg) {
      statusTimer.current = setTimeout(() => setStatus(''), durationMs)
    }
  }, [])

  const bridge = useJuceBridge()
  const didAutoSelect = useRef(false)

  // Auto-select Ibn Sina as default, falling back to first system
  const autoSelectFirst = useCallback((systems: TuningSystem[]) => {
    if (didAutoSelect.current || systems.length === 0) return
    didAutoSelect.current = true
    const preferred = systems.find(s => s.id === 'ibnsina_1037') ?? systems[0]
    const note = preferred.startingNotes[0]?.id ?? ''
    showStatus('Loading ' + (preferred.shortName || preferred.id) + '…', 10000)
    bridge.selectTuningSystem(preferred.id, note)
  }, [bridge, showStatus])

  // ── Load tuning systems on mount ──────────────────────────────────────────
  useEffect(() => {
    bridge.getTuningSystems().then(systems => {
      if (systems.length > 0) {
        setTuningSystems(systems)
        autoSelectFirst(systems)
      }
    })
  }, [bridge, autoSelectFirst])

  // ── Listen for C++ push events ────────────────────────────────────────────
  const onTuningSystemsLoaded = useCallback((data: unknown) => {
    if (Array.isArray(data)) {
      const systems = data as TuningSystem[]
      setTuningSystems(systems)
      autoSelectFirst(systems)
    }
  }, [autoSelectFirst])

  const onTuningStateChanged = useCallback((data: unknown) => {
    if (data && typeof data === 'object') setTuningState(data as TuningState)
  }, [])

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
  const bankContainerRef = useRef<HTMLDivElement>(null)
  const visibleCount = useVisibleSliderCount(bankContainerRef)

  // Clamp startMidi when visibleCount changes (e.g. window widened past MIDI range end)
  useEffect(() => {
    const maxStart = 128 - visibleCount
    if (startMidi > maxStart) setStartMidi(maxStart)
  }, [visibleCount, startMidi])

  const handleBankWheel = useCallback((e: React.WheelEvent) => {
    const delta = e.deltaY / SLOT_WIDTH_PX
    setStartMidi(prev => Math.max(0, Math.min(128 - visibleCount, prev + delta)))
  }, [visibleCount])

  // ── MIDI activity (note on / note off tracking) ─────────────────────────
  const [midiActiveNotes, setMidiActiveNotes] = useState<Set<number>>(new Set())

  const onMidiActivity = useCallback((data: unknown) => {
    if (!data || typeof data !== 'object') return
    const { on, off } = data as { on?: number[]; off?: number[] }
    const onNotes  = Array.isArray(on)  ? on  : []
    const offNotes = Array.isArray(off) ? off : []
    if (!onNotes.length && !offNotes.length) return
    setMidiActiveNotes(prev => {
      const next = new Set(prev)
      for (const n of onNotes)  next.add(n)
      for (const n of offNotes) next.delete(n)
      return next
    })
  }, [])

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
    maqamListRequested.current = false
    await bridge.selectTuningSystem(systemId, startingNote)
  }

  const handleSliderChange = async (
    chromaticIndex: number, variantIndex: number,
    midiNote: number, perNoteOnly: boolean
  ) => {
    const newState = perNoteOnly
      ? await bridge.setNoteVariant(midiNote, variantIndex)
      : await bridge.setSliderVariant(chromaticIndex, variantIndex)
    if (newState) setTuningState(newState)
    // Manual slider change breaks preset/maqam association
    setActivePresetIndex(-1)
    setMaqamDegreeIndices(EMPTY_SET)
    setMaqamTonicIndex(-1)
    setMaqamTonicMidi(-1)
  }

  const handleMaqamSelect = async (maqamId: string, transpositionIndex: number) => {
    setSelectedMaqamId(maqamId)
    setSelectedTransIdx(transpositionIndex)
    setActivePresetIndex(-1)
    const newState = await bridge.applyMaqam(maqamId, transpositionIndex)
    if (newState) {
      setTuningState(newState)
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
          setStartMidi(centerMaqamOctave(tonic.midi, visibleCount))
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

    await bridge.assignPreset(
      presetIndex, entry.maqamId, entry.maqamDisplay,
      entry.familyId, selectedTransIdx >= 0, tonicDisplay, tonicIpnLabel,
      selectedTransIdx, positions, degreeNames
    )
    // Update local preset state immediately
    setTuningState(prev => {
      const presets = [...prev.presets]
      presets[presetIndex] = {
        isAssigned: true,
        maqamId: entry.maqamId,
        maqamDisplay: entry.maqamDisplay,
        tonicIpn: tonicIpnLabel,
        baseMaqamId: entry.familyId,
        isTransposed: selectedTransIdx >= 0,
        tonicNote: tonicDisplay,
        setIndex: selectedTransIdx,
        sliderPositions: positions,
        degreeNames,
      }
      return { ...prev, presets }
    })
    setActivePresetIndex(presetIndex)
  }

  const handlePresetClick = async (presetIndex: number) => {
    const newState = await bridge.applyPreset(presetIndex)
    if (newState) setTuningState(newState)
    setActivePresetIndex(presetIndex)
    // Sync dropdown selection to match preset's maqam
    const preset = tuningState.presets[presetIndex]
    if (preset?.isAssigned) {
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
            setStartMidi(centerMaqamOctave(tonic.midi, visibleCount))
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
          midiActiveNotes={midiActiveNotes}
          startMidi={startMidi}
          visibleCount={visibleCount}
          noteNames={tuningState.noteNames}
          perNoteOverrides={tuningState.perNoteOverrides}
          maqamDegreeIndices={maqamDegreeIndices}
          maqamTonicIndex={maqamTonicIndex}
          maqamTonicMidi={maqamTonicMidi}
          onSliderChange={handleSliderChange}
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
