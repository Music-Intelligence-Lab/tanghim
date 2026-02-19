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
  const sorted = useMemo(() =>
    [...systems].sort((a, b) => (a.shortName || a.displayName).localeCompare(b.shortName || b.displayName)),
    [systems]
  )
  const current = sorted.find(s => s.id === currentSystemId)

  const systemOptions = useMemo(() =>
    systems.length === 0
      ? [{ value: '', label: 'Loading…' }]
      : sorted.map(s => ({ value: s.id, label: s.shortName || s.displayName })),
    [systems, sorted]
  )

  const noteOptions = useMemo(() =>
    current?.startingNotes.map(n => ({ value: n.id, label: n.displayName })) ?? [],
    [current]
  )

  const handleSystemChange = (value: string) => {
    const sys = sorted.find(s => s.id === value)
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
      />

      {current && current.startingNotes.length > 1 && (
        <CustomSelect
          options={noteOptions}
          value={currentStartingNote}
          onChange={note => onSelect(currentSystemId, note)}
          className="select-note"
        />
      )}
    </div>
  )
}
