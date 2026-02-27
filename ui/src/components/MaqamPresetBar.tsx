import { useMemo } from 'react'
import type { MaqamPreset, MaqamListEntry } from '../types'
import MaqamPresetButton from './MaqamPresetButton'
import './MaqamPresetBar.css'

interface Props {
  presets: MaqamPreset[]
  activePresetIndex: number
  maqamList: MaqamListEntry[]
  midiLearnTarget: number  // -1 if not learning, 0-15 = preset index
  midiPresetNotes: number[]  // Per-preset MIDI note mappings, -1 = unmapped
  onPresetClick: (index: number) => void
  onSaveToPreset: (index: number) => void
  onPresetClear: (index: number) => void
  onMidiLearnStart: (index: number) => void
  onMidiLearnCancel: () => void
  onMidiNoteClear: (index: number) => void
}

/** Check if a preset's maqam (and transposition) exists in the current maqam list.
 *  When maqam list is empty (not yet loaded or loading after system switch),
 *  assigned presets are disabled until the list confirms compatibility. */
function isPresetCompatible(preset: MaqamPreset, maqamList: MaqamListEntry[]): boolean {
  if (!preset.isAssigned) return true
  if (maqamList.length === 0) return false  // not yet loaded — disable until confirmed
  const entry = maqamList.find(m => m.maqamId === preset.maqamId)
  if (!entry) return false
  // If it's a transposition, check the specific transposition index exists
  if (preset.isTransposed && preset.setIndex >= 0)
    return preset.setIndex < entry.transpositions.length
  return true
}

export default function MaqamPresetBar({
  presets, activePresetIndex, maqamList,
  midiLearnTarget, midiPresetNotes,
  onPresetClick, onSaveToPreset, onPresetClear,
  onMidiLearnStart, onMidiLearnCancel, onMidiNoteClear
}: Props) {
  const compatibility = useMemo(
    () => presets.map(p => isPresetCompatible(p, maqamList)),
    [presets, maqamList]
  )

  return (
    <div className="preset-bar">
      {presets.slice(0, 8).map((preset, i) => (
        <MaqamPresetButton
          key={i}
          preset={preset}
          index={i}
          isActive={i === activePresetIndex}
          isDisabled={!compatibility[i]}
          isMidiLearning={midiLearnTarget === i}
          midiNote={midiPresetNotes[i] ?? -1}
          onApply={onPresetClick}
          onSave={onSaveToPreset}
          onClear={onPresetClear}
          onMidiLearnStart={onMidiLearnStart}
          onMidiLearnCancel={onMidiLearnCancel}
          onMidiNoteClear={onMidiNoteClear}
        />
      ))}
    </div>
  )
}
