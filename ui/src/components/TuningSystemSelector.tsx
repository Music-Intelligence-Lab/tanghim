import { useMemo } from 'react'
import type { TuningSystem } from '../types'
import CustomSelect from './CustomSelect'
import './TuningSystemSelector.css'

interface Props {
  systems: TuningSystem[]
  currentSystemId: string
  currentStartingNote: string
  onSelect: (systemId: string, startingNote: string) => void
}

export default function TuningSystemSelector({ systems, currentSystemId, currentStartingNote, onSelect }: Props) {
  // Systems arrive pre-sorted chronologically by year from C++
  const current = systems.find(s => s.id === currentSystemId)

  const sorted = useMemo(() =>
    [...systems].sort((a, b) => a.year !== b.year ? a.year - b.year : a.displayName.localeCompare(b.displayName)),
    [systems]
  )

  const systemOptions = useMemo(() =>
    sorted.length === 0
      ? [{ value: '', label: 'Loading…' }]
      : sorted.map(s => {
          const cached = s.startingNotes[0]?.isCached ?? false
          return { value: s.id, label: s.displayName, suffix: cached ? '✓' : '↓', suffixClass: cached ? 'cached' : 'uncached' }
        }),
    [sorted]
  )

  const noteOptions = useMemo(() =>
    current?.startingNotes.map(n => ({
      value: n.id, label: n.displayName, suffix: n.isCached ? '✓' : '↓', suffixClass: n.isCached ? 'cached' : 'uncached'
    })) ?? [],
    [current]
  )

  const handleSystemChange = (value: string) => {
    const sys = systems.find(s => s.id === value)
    if (!sys) return
    const note = sys.startingNotes[0]?.id ?? ''
    onSelect(sys.id, note)
  }

  return (
    <div className="tuning-system-selector">
      <CustomSelect
        options={systemOptions}
        value={currentSystemId}
        onChange={handleSystemChange}
        className="select-system"
        placeholder="Select tanghīm…"
      />

      {current && current.startingNotes.length >= 1 && (
        <CustomSelect
          options={noteOptions}
          value={currentStartingNote}
          onChange={note => onSelect(currentSystemId, note)}
          className="select-note"
          disabled={current.startingNotes.length <= 1}
        />
      )}
    </div>
  )
}
