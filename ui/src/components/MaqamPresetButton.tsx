import type { MaqamPreset } from '../types'
import './MaqamPresetButton.css'

// Convert MIDI note number to note name (e.g., 60 → "C3")
function midiToNoteName(note: number): string {
  if (note < 0 || note > 127) return ''
  const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
  const octave = Math.floor(note / 12) - 1
  return noteNames[note % 12] + octave
}

interface Props {
  preset: MaqamPreset
  index: number
  isActive: boolean
  isDisabled: boolean
  isMidiLearning: boolean
  midiNote: number  // -1 = unmapped
  onApply: (index: number) => void
  onSave: (index: number) => void
  onClear: (index: number) => void
  onMidiLearnStart: (index: number) => void
  onMidiLearnCancel: () => void
  onMidiNoteClear: (index: number) => void
}

export default function MaqamPresetButton({
  preset, index, isActive, isDisabled,
  isMidiLearning, midiNote,
  onApply, onSave, onClear,
  onMidiLearnStart, onMidiLearnCancel, onMidiNoteClear
}: Props) {
  const handleClick = (e: React.MouseEvent) => {
    // Shift+click to enter/exit MIDI Learn mode
    if (e.shiftKey) {
      if (isMidiLearning) {
        onMidiLearnCancel()
      } else {
        onMidiLearnStart(index)
      }
      return
    }

    if (isDisabled) return
    if (isActive) onApply(-1)
    else if (preset.isAssigned) onApply(index)
    else onSave(index)
  }

  const handleMidiBadgeClick = (e: React.MouseEvent) => {
    e.stopPropagation()
    // Click to clear, Shift+click to re-learn
    if (e.shiftKey) {
      onMidiLearnStart(index)
    } else {
      onMidiNoteClear(index)
    }
  }

  const tonicLine = preset.tonicNote && preset.tonicIpn
    ? `${preset.tonicNote} | ${preset.tonicIpn}${preset.tonicSolfege ? ' | ' + preset.tonicSolfege : ''}`
    : preset.tonicNote || ''

  const midiNoteName = midiNote >= 0 ? midiToNoteName(midiNote) : null

  return (
    <button
      className={`preset-btn ${preset.isAssigned ? 'assigned' : 'empty'} ${isActive ? 'active' : ''} ${isDisabled ? 'disabled' : ''} ${isMidiLearning ? 'learning' : ''}`}
      onClick={handleClick}
      title={
        isMidiLearning
          ? 'Press a MIDI note to map to this preset (Shift+click to cancel)'
          : isDisabled
            ? `${preset.maqamDisplay} — not available in this tuning system`
            : preset.isAssigned
              ? `${preset.maqamDisplay}${tonicLine ? ' ' + tonicLine : ''} (Shift+click for MIDI Learn)`
              : `Preset ${index + 1} — click to save (Shift+click for MIDI Learn)`
      }
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
      {/* MIDI note badge - shows when mapped, or "..." when learning */}
      {(midiNoteName || isMidiLearning) && (
        <span
          className={`preset-midi-badge ${isMidiLearning ? 'learning' : ''}`}
          onClick={handleMidiBadgeClick}
          title={isMidiLearning ? 'Learning...' : `MIDI: ${midiNoteName} (click to clear, Shift+click to re-learn)`}
        >
          {isMidiLearning ? '...' : midiNoteName}
        </span>
      )}
    </button>
  )
}
