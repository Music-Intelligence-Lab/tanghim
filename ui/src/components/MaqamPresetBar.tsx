import { useMemo } from 'react'
import type { MaqamPreset, MaqamListEntry } from '../types'
import MaqamPresetButton from './MaqamPresetButton'
import './MaqamPresetBar.css'

interface Props {
  presets: MaqamPreset[]
  activePresetIndex: number
  maqamList: MaqamListEntry[]
  onPresetClick: (index: number) => void
  onSaveToPreset: (index: number) => void
  onPresetClear: (index: number) => void
}

/** Check if a preset's maqam (and transposition) exists in the current maqam list. */
function isPresetCompatible(preset: MaqamPreset, maqamList: MaqamListEntry[]): boolean {
  if (!preset.isAssigned) return true
  const entry = maqamList.find(m => m.maqamId === preset.maqamId)
  if (!entry) return false
  // If it's a transposition, check the specific transposition index exists
  if (preset.isTransposed && preset.setIndex >= 0)
    return preset.setIndex < entry.transpositions.length
  return true
}

export default function MaqamPresetBar({ presets, activePresetIndex, maqamList, onPresetClick, onSaveToPreset, onPresetClear }: Props) {
  const compatibility = useMemo(
    () => presets.map(p => isPresetCompatible(p, maqamList)),
    [presets, maqamList]
  )

  return (
    <div className="preset-bar">
      {presets.map((preset, i) => (
        <MaqamPresetButton
          key={i}
          preset={preset}
          index={i}
          isActive={i === activePresetIndex}
          isDisabled={!compatibility[i]}
          onApply={onPresetClick}
          onSave={onSaveToPreset}
          onClear={onPresetClear}
        />
      ))}
    </div>
  )
}
