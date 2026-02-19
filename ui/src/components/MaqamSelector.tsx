import { useMemo } from 'react'
import type { MaqamListEntry } from '../types'
import CustomSelect from './CustomSelect'
import './MaqamSelector.css'

interface Props {
  maqamList: MaqamListEntry[]
  selectedMaqamId: string
  selectedTranspositionIndex: number  // -1 = base (no transposition)
  paoNameMap: Record<string, number>
  onSelect: (maqamId: string, transpositionIndex: number) => void
}

export default function MaqamSelector({
  maqamList, selectedMaqamId, selectedTranspositionIndex, paoNameMap, onSelect,
}: Props) {
  // Simple alphabetical list of maqamat (no family grouping)
  const maqamOptions = useMemo(() => {
    const opts = maqamList
      .map(m => ({ value: m.maqamId, label: m.maqamDisplay }))
      .sort((a, b) => a.label.localeCompare(b.label))
    return opts
  }, [maqamList])

  // Build transposition options sorted by ascending chromatic pitch class order
  const transpositionOptions = useMemo(() => {
    if (!selectedMaqamId) return []
    const entry = maqamList.find(m => m.maqamId === selectedMaqamId)
    if (!entry || entry.transpositions.length === 0) return []

    // All options: base + transpositions, each with their chromatic index
    const all: { value: string; label: string; chromaticIdx: number }[] = [
      {
        value: '-1',
        label: entry.tonicDisplay + ' (base)',
        chromaticIdx: paoNameMap[entry.tonicId] ?? 0,
      },
    ]
    for (let i = 0; i < entry.transpositions.length; i++) {
      const t = entry.transpositions[i]
      all.push({
        value: String(i),
        label: t.tonicDisplay,
        chromaticIdx: paoNameMap[t.tonicId] ?? 0,
      })
    }

    // Sort by chromatic index (ascending pitch class order)
    all.sort((a, b) => a.chromaticIdx - b.chromaticIdx)

    return all.map(({ value, label }) => ({ value, label }))
  }, [selectedMaqamId, maqamList, paoNameMap])

  const handleMaqamChange = (maqamId: string) => {
    onSelect(maqamId, -1) // default to base when switching maqam
  }

  const handleTranspositionChange = (indexStr: string) => {
    onSelect(selectedMaqamId, parseInt(indexStr, 10))
  }

  return (
    <div className="maqam-selector">
      <CustomSelect
        options={maqamOptions}
        value={selectedMaqamId}
        onChange={handleMaqamChange}
        className="select-maqam"
        placeholder="Select maqām…"
        searchable
      />
      {transpositionOptions.length > 0 && (
        <CustomSelect
          options={transpositionOptions}
          value={String(selectedTranspositionIndex)}
          onChange={handleTranspositionChange}
          className="select-transposition"
          placeholder="Transposition…"
          searchable
        />
      )}
    </div>
  )
}
