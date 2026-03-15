import { useCallback, useSyncExternalStore } from 'react'

/**
 * Lightweight external store for per-slot centsOffset overrides.
 * Allows individual NoteSliders to subscribe to only their own slot,
 * so automation/drag updates re-render just the affected slider.
 */
export function createSlotCentsStore() {
  const overrides = new Map<number, number>()
  // Per-slot subscriber sets: index → Set<callback>
  const subscribers = new Map<number, Set<() => void>>()

  function notify(index: number) {
    const subs = subscribers.get(index)
    if (subs) subs.forEach(cb => cb())
  }

  function set(index: number, cents: number) {
    overrides.set(index, cents)
    notify(index)
  }

  function clear() {
    const indices = [...overrides.keys()]
    overrides.clear()
    indices.forEach(notify)
  }

  function subscribe(index: number, callback: () => void): () => void {
    let subs = subscribers.get(index)
    if (!subs) {
      subs = new Set()
      subscribers.set(index, subs)
    }
    subs.add(callback)
    return () => { subs!.delete(callback) }
  }

  function getSnapshot(index: number): number | undefined {
    return overrides.get(index)
  }

  return { set, clear, subscribe, getSnapshot }
}

export type SlotCentsStore = ReturnType<typeof createSlotCentsStore>

/**
 * Hook for a single NoteSlider to subscribe to its chromatic index override.
 * Returns undefined when no override is active (use slot.centsOffset from state).
 */
export function useSlotCentsOverride(
  store: SlotCentsStore,
  chromaticIndex: number
): number | undefined {
  const subscribe = useCallback(
    (cb: () => void) => store.subscribe(chromaticIndex, cb),
    [store, chromaticIndex]
  )
  const getSnapshot = useCallback(
    () => store.getSnapshot(chromaticIndex),
    [store, chromaticIndex]
  )
  return useSyncExternalStore(subscribe, getSnapshot)
}
