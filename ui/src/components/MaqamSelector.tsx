import { useMemo } from 'react'
import type { MaqamListEntry } from '../types'
import CustomSelect from './CustomSelect'
import './MaqamSelector.css'

interface Props {
  maqamList: MaqamListEntry[]
  selectedMaqamId: string
  selectedTranspositionIndex: number  // -1 = base (no transposition)
  paoOrder: string[]                  // unique PAO idNames in ascending MIDI note order
  paoNameInfo: Record<string, { englishName: string; solfege: string; octave: number }>
  isModified?: boolean                // true when slider tuning has been adjusted
  activePresetIndex: number            // -1 = no preset active
  onSelect: (maqamId: string, transpositionIndex: number) => void
}

/** Build a display label like "segāh / E-b3 / Mi -b3 (qarār)" */
function buildTonicLabel(
  tonicId: string,
  tonicDisplay: string,
  paoNameInfo: Props['paoNameInfo'],
  isBase: boolean,
): string {
  const info = paoNameInfo[tonicId]
  let label = tonicDisplay
  if (info) {
    const parts = [info.englishName, info.solfege].filter(Boolean)
    if (parts.length > 0) label += ' / ' + parts.join(' / ')
  }
  if (isBase) label += ' (qarār)'
  return label
}

export default function MaqamSelector({
  maqamList, selectedMaqamId, selectedTranspositionIndex, paoOrder, paoNameInfo, isModified, activePresetIndex, onSelect,
}: Props) {
  // Gold fill on triggers when maqam+tonic selected but no preset active
  const goldFill = selectedMaqamId && activePresetIndex < 0
  // Simple alphabetical list of maqamat (no family grouping)
  const maqamOptions = useMemo(() => {
    const opts = maqamList
      .map(m => ({ value: m.maqamId, label: m.maqamDisplay }))
      .sort((a, b) => a.label.localeCompare(b.label))
    return opts
  }, [maqamList])

  // Build transposition options sorted by the tuning system's pitch class order (low to high)
  const transpositionOptions = useMemo(() => {
    if (!selectedMaqamId) return []
    const entry = maqamList.find(m => m.maqamId === selectedMaqamId)
    if (!entry || entry.transpositions.length === 0) return []

    // Build lookup: PAO idName → position in ascending MIDI order
    const orderMap = new Map(paoOrder.map((name, i) => [name, i]))

    // Only show transpositions in octaves 1–2 (skip qarār/octave 0 and jawāb/octave 3+)
    const isInRange = (id: string) => {
      const oct = paoNameInfo[id]?.octave
      return oct === 1 || oct === 2
    }

    // All options: base (always shown) + transpositions (filtered to octaves 1–2)
    const all: { value: string; label: string; order: number }[] = [
      {
        value: '-1',
        label: buildTonicLabel(entry.tonicId, entry.tonicDisplay, paoNameInfo, true),
        order: orderMap.get(entry.tonicId) ?? 999,
      },
    ]
    for (let i = 0; i < entry.transpositions.length; i++) {
      const t = entry.transpositions[i]
      if (!isInRange(t.tonicId)) continue
      all.push({
        value: String(i),
        label: buildTonicLabel(t.tonicId, t.tonicDisplay, paoNameInfo, false),
        order: orderMap.get(t.tonicId) ?? 999,
      })
    }

    // Sort by position in the tuning system's pitch class data (ascending MIDI note order)
    all.sort((a, b) => a.order - b.order)

    return all.map(({ value, label }) => ({ value, label }))
  }, [selectedMaqamId, maqamList, paoOrder, paoNameInfo])

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
        className={`select-maqam${goldFill ? ' gold-fill' : ''}`}
        placeholder="Select maqām…"
        searchable
        selectedSuffix={isModified ? ' *' : undefined}
      />
      {transpositionOptions.length > 0 && (
        <CustomSelect
          options={transpositionOptions}
          value={String(selectedTranspositionIndex)}
          onChange={handleTranspositionChange}
          className={`select-transposition${goldFill ? ' gold-fill' : ''}`}
          placeholder="Transposition…"
          searchable
        />
      )}
    </div>
  )
}
