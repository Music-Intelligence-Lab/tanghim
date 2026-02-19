import { useRef } from 'react'
import type { ChromaticSlot } from '../types'
import './NoteSlider.css'

interface Props {
  slot: ChromaticSlot
  chromaticIndex: number
  midiNote: number
  effectiveIndex: number
  hasOverride: boolean
  midiActive: boolean
  isMaqamDegree: boolean
  isMaqamDegreeEquiv: boolean
  isMaqamTonic: boolean
  isMaqamTonicEquiv: boolean
  ipnLabel: string       // e.g. "C3", "A4"
  paoName: string        // e.g. "rāst", "—"
  onVariantChange: (chromaticIndex: number, variantIndex: number, midiNote: number, perNoteOnly: boolean) => void
}

export default function NoteSlider({ slot, chromaticIndex, midiNote, effectiveIndex, hasOverride, midiActive, isMaqamDegree, isMaqamDegreeEquiv, isMaqamTonic, isMaqamTonicEquiv, ipnLabel, paoName, onVariantChange }: Props) {
  const trackRef = useRef<HTMLDivElement>(null)
  const dragging  = useRef(false)

  const variantCount = slot.variants.length
  const isLocked     = slot.isLocked || variantCount <= 1

  // ── Deviation-based positioning ────────────────────────────────────────────
  // 0 cents deviation = 50% (vertical center of track)
  // Positive deviation (sharper) → above center (lower %)
  // Negative deviation (flatter) → below center (higher %)
  // Fixed ±100 cents range so positions stay consistent across tuning systems
  const devToPct = (dev: number) => 50 - (dev / 100) * 45

  // Pre-compute positions for each variant
  const variantPcts = slot.variants.map(v => devToPct(v.midiCentsDeviation))

  /** Find the variant whose track position is closest to the click Y. */
  const yToVariantIndex = (clientY: number): number => {
    if (!trackRef.current || variantCount <= 1) return 0
    const rect = trackRef.current.getBoundingClientRect()
    const clickPct = ((clientY - rect.top) / rect.height) * 100

    let bestIdx = 0
    let bestDist = Infinity
    for (let i = 0; i < variantPcts.length; i++) {
      const dist = Math.abs(variantPcts[i] - clickPct)
      if (dist < bestDist) {
        bestDist = dist
        bestIdx = i
      }
    }
    return bestIdx
  }

  const handleMouseDown = (e: React.MouseEvent) => {
    if (isLocked) return
    dragging.current = true
    const perNoteOnly = e.shiftKey  // Shift = per-note override only
    const newIdx = yToVariantIndex(e.clientY)
    if (newIdx !== effectiveIndex) onVariantChange(chromaticIndex, newIdx, midiNote, perNoteOnly)

    const onMove = (ev: MouseEvent) => {
      if (!dragging.current) return
      const idx = yToVariantIndex(ev.clientY)
      if (idx !== effectiveIndex) onVariantChange(chromaticIndex, idx, midiNote, perNoteOnly)
    }
    const onUp = () => { dragging.current = false; window.removeEventListener('mousemove', onMove); window.removeEventListener('mouseup', onUp) }
    window.addEventListener('mousemove', onMove)
    window.addEventListener('mouseup', onUp)
  }

  const selected = slot.variants[effectiveIndex]
  const thumbPct = selected ? devToPct(selected.midiCentsDeviation) : 50

  return (
    <div className={`note-slider ${isLocked ? 'locked' : ''} ${hasOverride ? 'has-override' : ''} ${isMaqamDegree ? 'maqam-degree' : ''} ${isMaqamDegreeEquiv ? 'maqam-degree-equiv' : ''} ${isMaqamTonic ? 'maqam-tonic' : ''} ${isMaqamTonicEquiv ? 'maqam-tonic-equiv' : ''}`}>
      <div className="ipn-label">{ipnLabel}</div>

      <div className="track-wrap" ref={trackRef} onMouseDown={handleMouseDown}>
        <div className="track">
          {/* Snap markers on the left */}
          <div className="snap-markers">
            {variantPcts.map((pct, i) => (
              <div
                key={i}
                className={`snap-marker ${i === effectiveIndex ? 'active' : ''}`}
                style={{ top: `${pct}%` }}
              />
            ))}
          </div>
          {/* Thumb */}
          <div
            className={`thumb ${midiActive ? 'midi-hit' : ''}`}
            style={{ top: `${thumbPct}%` }}
          />
        </div>
      </div>

      {/* Cents deviation above note name */}
      {selected && (
        <div className="cents">
          {selected.midiCentsDeviation >= 0 ? '+' : ''}{selected.midiCentsDeviation.toFixed(1)}¢
        </div>
      )}

      {/* PAO note name — allowed to wrap to two lines, break at "/" */}
      <div className="note-name" title={selected?.englishName}>
        {paoName.includes('/') ? paoName.split('/').map((part, i, arr) => (
          <span key={i}>{part}{i < arr.length - 1 && <>/<wbr/></>}</span>
        )) : paoName}
      </div>
    </div>
  )
}
