import type { MaqamPreset } from '../types'
import './MaqamPresetButton.css'

interface Props {
  preset: MaqamPreset
  index: number
  isActive: boolean
  isDisabled: boolean
  onApply: (index: number) => void
  onSave: (index: number) => void
  onClear: (index: number) => void
}

export default function MaqamPresetButton({ preset, index, isActive, isDisabled, onApply, onSave, onClear }: Props) {
  const handleClick = () => {
    if (isDisabled) return
    if (preset.isAssigned) onApply(index)
    else onSave(index)
  }

  const tonicLine = preset.tonicNote && preset.tonicIpn
    ? `${preset.tonicNote} | ${preset.tonicIpn}${preset.tonicSolfege ? ' | ' + preset.tonicSolfege : ''}`
    : preset.tonicNote || ''

  return (
    <button
      className={`preset-btn ${preset.isAssigned ? 'assigned' : 'empty'} ${isActive ? 'active' : ''} ${isDisabled ? 'disabled' : ''}`}
      onClick={handleClick}
      title={isDisabled
        ? `${preset.maqamDisplay} — not available in this tuning system`
        : preset.isAssigned
          ? `${preset.maqamDisplay}${tonicLine ? ' ' + tonicLine : ''}`
          : `Preset ${index + 1} — click to save`}
    >
      <span className="preset-num">{index + 1}</span>
      {preset.isAssigned && (
        <>
          <span className="preset-label">{(preset.maqamDisplay || preset.maqamId).replace(/\bal-/gi, 'al\u2011')}</span>
          {tonicLine && <span className="preset-tonic">{tonicLine}</span>}
        </>
      )}
      {preset.isAssigned && (
        <span
          className="preset-clear-btn"
          onClick={e => { e.stopPropagation(); onClear(index) }}
          title="Clear preset"
        >×</span>
      )}
    </button>
  )
}
