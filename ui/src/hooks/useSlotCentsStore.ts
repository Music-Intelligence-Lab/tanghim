import { useCallback, useSyncExternalStore } from 'react'

/**
 * Lightweight external store for per-slot and per-note centsOffset overrides.
 * Allows individual NoteSliders to subscribe to only their own slot/note,
 * so automation/drag updates re-render just the affected slider.
 *
 * Two namespaces:
 * - Chromatic slot overrides (keys 0-11): from DAW automation or normal drag
 * - Per-note overrides (keys 128+midiNote): from Shift+drag
 */
export function createSlotCentsStore() {
  const overrides = new Map<number, number>()
  const subscribers = new Map<number, Set<() => void>>()

  function notify(key: number) {
    const subs = subscribers.get(key)
    if (subs) subs.forEach(cb => cb())
  }

  function set(key: number, cents: number) {
    overrides.set(key, cents)
    notify(key)
  }

  /** Set a chromatic slot override (all octaves). */
  function setSlot(chromaticIndex: number, cents: number) {
    set(chromaticIndex, cents)
  }

  /** Set a per-MIDI-note override (single note). */
  function setNote(midiNote: number, cents: number) {
    set(128 + midiNote, cents)
  }

  function clear() {
    const keys = [...overrides.keys()]
    overrides.clear()
    keys.forEach(notify)
  }

  function subscribe(key: number, callback: () => void): () => void {
    let subs = subscribers.get(key)
    if (!subs) {
      subs = new Set()
      subscribers.set(key, subs)
    }
    subs.add(callback)
    return () => { subs!.delete(callback) }
  }

  function getSnapshot(key: number): number | undefined {
    return overrides.get(key)
  }

  return { setSlot, setNote, clear, subscribe, getSnapshot }
}

export type SlotCentsStore = ReturnType<typeof createSlotCentsStore>

/**
 * Hook for a single NoteSlider to subscribe to both its chromatic slot
 * and its per-MIDI-note override. Per-note takes priority.
 * Returns undefined when no override is active.
 */
export function useSlotCentsOverride(
  store: SlotCentsStore,
  chromaticIndex: number,
  midiNote: number
): number | undefined {
  // Subscribe to both the chromatic slot and the per-note key
  const subscribe = useCallback(
    (cb: () => void) => {
      const unsub1 = store.subscribe(chromaticIndex, cb)
      const unsub2 = store.subscribe(128 + midiNote, cb)
      return () => { unsub1(); unsub2() }
    },
    [store, chromaticIndex, midiNote]
  )
  const getSnapshot = useCallback(
    () => {
      // Per-note override takes priority over chromatic slot override
      const noteOverride = store.getSnapshot(128 + midiNote)
      if (noteOverride !== undefined) return noteOverride
      return store.getSnapshot(chromaticIndex)
    },
    [store, chromaticIndex, midiNote]
  )
  return useSyncExternalStore(subscribe, getSnapshot)
}
