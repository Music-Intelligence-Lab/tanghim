import { useEffect, useCallback, useRef, useMemo } from 'react'
import type { TuningState, TuningSystem, MaqamListEntry, MaqamMidiDragData } from '../types'

// ── JUCE native integration (no ./juce module import needed) ─────────────────
//
// JUCE 8 injects `window.__JUCE__` via user script, which works regardless of
// whether the page is served from the resource provider or an external URL
// (like a Vite dev server).
//
// The `./juce` ES module is only available via the resource provider, so we
// replicate its `getNativeFunction` logic here using the low-level
// `window.__JUCE__.backend` API. This is based on the official JUCE frontend
// code in juce_gui_extra/native/javascript/index.js.

/** Whether we are running inside the JUCE WebView (vs. standalone browser dev). */
export const isInJuce = () =>
  typeof window.__JUCE__ !== 'undefined' && typeof window.__JUCE__?.backend !== 'undefined'

// ── Promise handler for native function calls ────────────────────────────────

let promiseHandlerInitialised = false
let lastPromiseId = 0
const pendingPromises = new Map<number, { resolve: (v: unknown) => void }>()

/** Set up the completion listener once. */
function ensurePromiseHandler() {
  if (promiseHandlerInitialised || !isInJuce()) return
  promiseHandlerInitialised = true

  window.__JUCE__!.backend.addEventListener('__juce__complete',
    (event: unknown) => {
      const { promiseId, result } = event as { promiseId: number; result: unknown }
      const entry = pendingPromises.get(promiseId)
      if (entry) {
        entry.resolve(result)
        pendingPromises.delete(promiseId)
      }
    }
  )
}

/**
 * Replicate JUCE's `getNativeFunction` without needing the `./juce` module.
 * Returns a function that, when called, emits `__juce__invoke` and returns
 * a Promise that resolves when the C++ side calls the completion callback.
 */
function getNativeFunction(name: string): (...args: unknown[]) => Promise<unknown> {
  ensurePromiseHandler()

  return (...args: unknown[]) => {
    const promiseId = lastPromiseId++
    const promise = new Promise<unknown>((resolve) => {
      pendingPromises.set(promiseId, { resolve })
    })

    window.__JUCE__!.backend.emitEvent('__juce__invoke', {
      name,
      params: args,
      resultId: promiseId,
    })

    return promise
  }
}

/** Safely call a JUCE native function. Returns undefined if not in JUCE. */
async function callNative<T>(name: string, ...args: unknown[]): Promise<T | undefined> {
  if (!isInJuce()) return undefined
  const fn = getNativeFunction(name)
  return fn(...args) as Promise<T>
}

// ── Event subscriptions ───────────────────────────────────────────────────────

/**
 * Subscribe to events pushed from C++ via emitEventIfBrowserIsVisible.
 *
 * JUCE 8 event API:
 *   window.__JUCE__.backend.addEventListener(eventId, callback)
 *
 * The callback receives the data directly (already a JS object),
 * NOT wrapped in { data: string }.
 */
export function useJuceEvent(event: string, handler: (data: unknown) => void) {
  const handlerRef = useRef(handler)
  handlerRef.current = handler

  useEffect(() => {
    if (!isInJuce()) return

    const wrapped = (data: unknown) => {
      handlerRef.current(data)
    }

    window.__JUCE__!.backend.addEventListener(event, wrapped)
    return () => window.__JUCE__!.backend.removeEventListener(event, wrapped)
  }, [event])
}

// ── API calls ─────────────────────────────────────────────────────────────────

export function useJuceBridge() {
  const getTuningSystems = useCallback(async (): Promise<TuningSystem[]> => {
    return (await callNative<TuningSystem[]>('getTuningSystems')) ?? []
  }, [])

  const selectTuningSystem = useCallback(async (systemId: string, startingNote: string) => {
    await callNative('selectTuningSystem', systemId, startingNote)
  }, [])

  const setSliderVariant = useCallback(async (
    chromaticIndex: number,
    variantIndex: number
  ): Promise<TuningState | undefined> => {
    return callNative<TuningState>('setSliderVariant', chromaticIndex, variantIndex)
  }, [])

  const setNoteVariant = useCallback(async (
    midiNote: number,
    variantIndex: number
  ): Promise<TuningState | undefined> => {
    return callNative<TuningState>('setNoteVariant', midiNote, variantIndex)
  }, [])

  const setSlotCents = useCallback(async (
    chromaticIndex: number,
    centsValue: number
  ): Promise<void> => {
    await callNative('setSlotCents', chromaticIndex, centsValue)
  }, [])

  const setSlotCentsFinalize = useCallback(async (
    chromaticIndex: number,
    centsValue: number
  ): Promise<TuningState | undefined> => {
    return callNative<TuningState>('setSlotCentsFinalize', chromaticIndex, centsValue)
  }, [])

  const applyPreset = useCallback(async (presetIndex: number): Promise<TuningState | undefined> => {
    return callNative<TuningState>('applyPreset', presetIndex)
  }, [])

  const assignPreset = useCallback(async (
    presetIndex: number,
    maqamId: string,
    maqamDisplay: string,
    baseMaqamId: string,
    isTransposed: boolean,
    tonicNote: string,
    tonicIpn: string,
    tonicSolfege: string,
    setIndex: number,
    sliderPositions: number[],
    degreeNames: string[],
    centsOffsets: number[],
    tuningSystemId: string,
    startingNote: string
  ) => {
    await callNative('assignPreset', presetIndex, maqamId, maqamDisplay,
      baseMaqamId, isTransposed, tonicNote, tonicIpn, tonicSolfege, setIndex, sliderPositions, degreeNames, centsOffsets,
      tuningSystemId, startingNote)
  }, [])

  const clearPreset = useCallback(async (presetIndex: number) => {
    await callNative('clearPreset', presetIndex)
  }, [])

  const getMaqamList = useCallback(async (): Promise<MaqamListEntry[]> => {
    return (await callNative<MaqamListEntry[]>('getMaqamList')) ?? []
  }, [])

  const applyMaqam = useCallback(async (
    maqamId: string,
    transpositionIndex: number
  ): Promise<TuningState | undefined> => {
    return callNative<TuningState>('applyMaqam', maqamId, transpositionIndex)
  }, [])

  const checkForUpdates = useCallback(async (): Promise<string[]> => {
    return (await callNative<string[]>('checkForUpdates')) ?? []
  }, [])

  const getCurrentState = useCallback(async (): Promise<TuningState | undefined> => {
    return callNative<TuningState>('getCurrentState')
  }, [])

  const setStartMidi = useCallback(async (value: number) => {
    await callNative('setStartMidi', value)
  }, [])

  /** Get MIDI file data for current maqam (for drag export). */
  const getMaqamMidiDragData = useCallback(async (): Promise<MaqamMidiDragData | undefined> => {
    return callNative<MaqamMidiDragData>('getMaqamMidiDragData')
  }, [])

  /** Save MIDI file to Downloads folder. */
  const saveMaqamMidiFile = useCallback(async (): Promise<{ path: string; filename: string } | undefined> => {
    return callNative<{ path: string; filename: string }>('saveMaqamMidiFile')
  }, [])

  // ── Reference frequency control ──────────────────────────────────────────

  /** Fire-and-forget: update reference freq offset (live MTS-ESP update). */
  const setReferenceFreqCents = useCallback(async (cents: number): Promise<void> => {
    await callNative('setReferenceFreqCents', cents)
  }, [])

  /** Finalize reference freq offset (returns full TuningState). */
  const setReferenceFreqCentsFinalize = useCallback(async (cents: number): Promise<TuningState | undefined> => {
    return callNative<TuningState>('setReferenceFreqCentsFinalize', cents)
  }, [])

  /** Begin ref freq gesture for DAW automation. */
  const beginRefFreqGesture = useCallback(async (): Promise<void> => {
    await callNative('beginRefFreqGesture')
  }, [])

  /** End ref freq gesture for DAW automation. */
  const endRefFreqGesture = useCallback(async (): Promise<void> => {
    await callNative('endRefFreqGesture')
  }, [])

  // ── Gesture marking for DAW automation recording ─────────────────────────

  /** Begin slider gesture (call on mousedown for proper DAW automation). */
  const beginSliderGesture = useCallback(async (chromaticIndex: number): Promise<void> => {
    await callNative('beginSliderGesture', chromaticIndex)
  }, [])

  /** End slider gesture (call on mouseup for proper DAW automation). */
  const endSliderGesture = useCallback(async (chromaticIndex: number): Promise<void> => {
    await callNative('endSliderGesture', chromaticIndex)
  }, [])

  /** Begin preset gesture (call before preset selection). */
  const beginPresetGesture = useCallback(async (): Promise<void> => {
    await callNative('beginPresetGesture')
  }, [])

  /** End preset gesture (call after preset selection). */
  const endPresetGesture = useCallback(async (): Promise<void> => {
    await callNative('endPresetGesture')
  }, [])

  // ── MIDI Learn (per-preset note mapping) ─────────────────────────────────

  /** Start MIDI Learn for a preset. Next MIDI note received will be mapped to this preset. */
  const startMidiLearn = useCallback(async (presetIndex: number): Promise<number | undefined> => {
    return callNative<number>('startMidiLearn', presetIndex)
  }, [])

  /** Cancel MIDI Learn mode. */
  const cancelMidiLearn = useCallback(async (): Promise<void> => {
    await callNative('cancelMidiLearn')
  }, [])

  /** Get preset index currently in MIDI Learn mode (-1 if none). */
  const getMidiLearnTarget = useCallback(async (): Promise<number | undefined> => {
    return callNative<number>('getMidiLearnTarget')
  }, [])

  /** Get the MIDI note mapped to a preset (-1 if unmapped). */
  const getMidiPresetNote = useCallback(async (presetIndex: number): Promise<number | undefined> => {
    return callNative<number>('getMidiPresetNote', presetIndex)
  }, [])

  /** Clear the MIDI note mapping for a preset. */
  const clearMidiPresetNote = useCallback(async (presetIndex: number): Promise<void> => {
    await callNative('clearMidiPresetNote', presetIndex)
  }, [])

  /** Clear all MIDI preset note mappings. */
  const clearAllMidiPresetNotes = useCallback(async (): Promise<void> => {
    await callNative('clearAllMidiPresetNotes')
  }, [])

  /** Set MIDI channel for preset triggering. 0 = any channel, 1-16 = specific. */
  const setMidiPresetChannel = useCallback(async (channel: number): Promise<number | undefined> => {
    return callNative<number>('setMidiPresetChannel', channel)
  }, [])

  /** Get current MIDI channel for preset triggering. */
  const getMidiPresetChannel = useCallback(async (): Promise<number | undefined> => {
    return callNative<number>('getMidiPresetChannel')
  }, [])

  return useMemo(() => ({
    getTuningSystems,
    selectTuningSystem,
    setSliderVariant,
    setNoteVariant,
    setSlotCents,
    setSlotCentsFinalize,
    setReferenceFreqCents,
    setReferenceFreqCentsFinalize,
    beginRefFreqGesture,
    endRefFreqGesture,
    applyPreset,
    assignPreset,
    clearPreset,
    getMaqamList,
    applyMaqam,
    checkForUpdates,
    getCurrentState,
    setStartMidi,
    getMaqamMidiDragData,
    saveMaqamMidiFile,
    beginSliderGesture,
    endSliderGesture,
    beginPresetGesture,
    endPresetGesture,
    startMidiLearn,
    cancelMidiLearn,
    getMidiLearnTarget,
    getMidiPresetNote,
    clearMidiPresetNote,
    clearAllMidiPresetNotes,
    setMidiPresetChannel,
    getMidiPresetChannel,
  }), [getTuningSystems, selectTuningSystem, setSliderVariant, setNoteVariant,
       setSlotCents, setSlotCentsFinalize,
       setReferenceFreqCents, setReferenceFreqCentsFinalize, beginRefFreqGesture, endRefFreqGesture,
       applyPreset, assignPreset, clearPreset,
       getMaqamList, applyMaqam, checkForUpdates, getCurrentState, setStartMidi, getMaqamMidiDragData, saveMaqamMidiFile,
       beginSliderGesture, endSliderGesture, beginPresetGesture, endPresetGesture,
       startMidiLearn, cancelMidiLearn, getMidiLearnTarget, getMidiPresetNote, clearMidiPresetNote, clearAllMidiPresetNotes,
       setMidiPresetChannel, getMidiPresetChannel])
}
