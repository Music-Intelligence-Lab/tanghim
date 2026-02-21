import { useRef, memo } from 'react'
import type { ChromaticSlot } from '../types'
import './NoteSlider.css'

interface Props {
  slot: ChromaticSlot
  chromaticIndex: number
  midiNote: number
  effectiveIndex: number
  hasOverride: boolean
  isMaqamDegree: boolean
  isMaqamDegreeEquiv: boolean
  isMaqamTonic: boolean
  isMaqamTonicEquiv: boolean
  isModified: boolean
  ipnLabel: string       // e.g. "C3", "A4"
  solfege: string        // e.g. "Mi -b3", "Do 2"
  paoName: string        // e.g. "rāst", "—"
  onVariantSelect: (chromaticIndex: number, variantIndex: number) => void
  onCentsDrag: (chromaticIndex: number, centsValue: number) => void
  onCentsDragEnd: (chromaticIndex: number, centsValue: number) => void
  onGestureStart?: (chromaticIndex: number) => void
  onGestureEnd?: (chromaticIndex: number) => void
}

const NoteSlider = memo(function NoteSlider({ slot, chromaticIndex, midiNote, effectiveIndex, hasOverride, isMaqamDegree, isMaqamDegreeEquiv, isMaqamTonic, isMaqamTonicEquiv, isModified, ipnLabel, solfege, paoName, onVariantSelect, onCentsDrag, onCentsDragEnd, onGestureStart, onGestureEnd }: Props) {
  const trackRef = useRef<HTMLDivElement>(null)
  const thumbRef = useRef<HTMLDivElement>(null)
  const dragging = useRef(false)

  const variantCount = slot.variants.length
  // Only lock if no variants at all (empty data) — with continuous tuning, even single-variant slots are adjustable
  const isLocked     = variantCount === 0

  // Derive centsOffset from selected variant if C++ hasn't sent it yet
  const centsOffset = slot.centsOffset ?? slot.variants[slot.selectedIndex]?.midiCentsDeviation ?? 0

  // ── Deviation-based positioning (±150 cents range) ──────────────────────────
  // 0 cents deviation = 50% (vertical center of track)
  // Positive deviation (sharper) → above center (lower %)
  // Negative deviation (flatter) → below center (higher %)
  const CENTS_RANGE = 150
  const TRACK_HALF_PCT = 45  // Use 45% of track on each side of center (5% to 95%) for thumb padding

  const devToPct = (dev: number) => {
    const pct = 50 - (dev / CENTS_RANGE) * TRACK_HALF_PCT
    return Math.max(5, Math.min(95, pct))  // Clamp to track bounds
  }

  // Inverse: convert track percentage to cents deviation
  const pctToDev = (pct: number): number => -(pct - 50) * CENTS_RANGE / TRACK_HALF_PCT

  // Pre-compute positions for each variant (for snap markers)
  const variantPcts = slot.variants.map(v => devToPct(v.midiCentsDeviation))

  /** Convert mouse clientY to a clamped cents value. */
  const yToCents = (clientY: number): number => {
    if (!trackRef.current) return 0
    const rect = trackRef.current.getBoundingClientRect()
    const pct = ((clientY - rect.top) / rect.height) * 100
    const cents = pctToDev(pct)
    return Math.max(-150, Math.min(150, cents))
  }

  /** Free drag on track/thumb — continuous cents update. */
  const handleMouseDown = (e: React.MouseEvent) => {
    if (isLocked) return
    dragging.current = true
    thumbRef.current?.classList.add('dragging')

    // Begin DAW automation gesture
    onGestureStart?.(chromaticIndex)

    const cents = yToCents(e.clientY)
    onCentsDrag(chromaticIndex, cents)

    const onMove = (ev: MouseEvent) => {
      if (!dragging.current) return
      const c = yToCents(ev.clientY)
      onCentsDrag(chromaticIndex, c)
    }
    const onUp = (ev: MouseEvent) => {
      dragging.current = false
      thumbRef.current?.classList.remove('dragging')
      const c = yToCents(ev.clientY)
      onCentsDragEnd(chromaticIndex, c)
      // End DAW automation gesture
      onGestureEnd?.(chromaticIndex)
      window.removeEventListener('mousemove', onMove)
      window.removeEventListener('mouseup', onUp)
    }
    window.addEventListener('mousemove', onMove)
    window.addEventListener('mouseup', onUp)
  }

  /** Snap marker click — select a specific variant. */
  const handleSnapMarkerClick = (e: React.MouseEvent, variantIndex: number) => {
    e.stopPropagation()
    if (isLocked) return
    onVariantSelect(chromaticIndex, variantIndex)
  }

  // Thumb position driven by centsOffset (source of truth)
  const thumbPct = devToPct(centsOffset)

  return (
    <div data-midi={midiNote} className={`note-slider ${isLocked ? 'locked' : ''} ${hasOverride ? 'has-override' : ''} ${isMaqamDegree ? 'maqam-degree' : ''} ${isMaqamDegreeEquiv ? 'maqam-degree-equiv' : ''} ${isMaqamTonic ? 'maqam-tonic' : ''} ${isMaqamTonicEquiv ? 'maqam-tonic-equiv' : ''} ${isModified ? 'modified' : ''}`}>
      <div className="track-wrap" ref={trackRef} onMouseDown={handleMouseDown}>
        <div className="track">
          {/* Snap markers on the left — clickable to snap to variant */}
          <div className="snap-markers">
            {variantPcts.map((pct, i) => {
              // Only show marker as active if selected AND slider value exactly matches
              const isActive = i === effectiveIndex && centsOffset === slot.variants[i].midiCentsDeviation
              return (
                <div
                  key={i}
                  className={`snap-marker ${isActive ? 'active' : ''}`}
                  style={{ top: `${pct}%` }}
                  onMouseDown={(e) => handleSnapMarkerClick(e, i)}
                />
              )
            })}
          </div>
          {/* Thumb */}
          <div
            ref={thumbRef}
            className="thumb"
            style={{ top: `${thumbPct}%` }}
          />
        </div>
      </div>

      {/* Cents deviation */}
      <div className="cents">
        {centsOffset >= 0 ? '+' : ''}{centsOffset.toFixed(1)}¢
      </div>

      {/* IPN label (e.g. "C3", "E-b3") */}
      <div className="ipn-label">{ipnLabel}</div>

      {/* Solfège (e.g. "Mi -b3", "Do 2") */}
      <div className="solfege">{solfege}</div>

      {/* PAO note name — allowed to wrap to two lines, break at "/" */}
      <div className="note-name" title={slot.variants[effectiveIndex]?.englishName}>
        {paoName.includes('/') ? paoName.split('/').map((part, i, arr) => (
          <span key={i}>{part}{i < arr.length - 1 && <>/<wbr/></>}</span>
        )) : paoName}
      </div>
    </div>
  )
})

export default NoteSlider
