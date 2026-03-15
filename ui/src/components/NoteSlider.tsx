import { useRef, memo } from 'react'
import type { ChromaticSlot } from '../types'
import { useSlotCentsOverride, type SlotCentsStore } from '../hooks/useSlotCentsStore'
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
  maqamVariantIndex: number  // variant index matching the maqam degree (-1 if none)
  isWhiteKey: boolean
  isHeptMuted: boolean
  heptSourceKey?: string    // white key letter that plays this degree (e.g. "E" for Eb)
  ipnLabel: string       // e.g. "C3", "A4"
  solfege: string        // e.g. "Mi -b3", "Do 2"
  paoName: string        // e.g. "rāst", "—"
  slotCentsStore: SlotCentsStore
  hasPerNoteCentsOverride: boolean
  perNoteCentsValue?: number  // per-note cents from tuning state (undefined = use slot)
  onVariantSelect: (chromaticIndex: number, variantIndex: number) => void
  onCentsDrag: (chromaticIndex: number, centsValue: number) => void
  onCentsDragEnd: (chromaticIndex: number, centsValue: number) => void
  onNoteCentsDrag: (midiNote: number, centsValue: number) => void
  onNoteCentsDragEnd: (midiNote: number, centsValue: number) => void
  onGestureStart?: (chromaticIndex: number) => void
  onGestureEnd?: (chromaticIndex: number) => void
}

function propsAreEqual(prev: Props, next: Props): boolean {
  return (
    prev.chromaticIndex === next.chromaticIndex &&
    prev.midiNote === next.midiNote &&
    prev.effectiveIndex === next.effectiveIndex &&
    prev.hasOverride === next.hasOverride &&
    prev.isMaqamDegree === next.isMaqamDegree &&
    prev.isMaqamDegreeEquiv === next.isMaqamDegreeEquiv &&
    prev.isMaqamTonic === next.isMaqamTonic &&
    prev.isMaqamTonicEquiv === next.isMaqamTonicEquiv &&
    prev.isModified === next.isModified &&
    prev.maqamVariantIndex === next.maqamVariantIndex &&
    prev.isWhiteKey === next.isWhiteKey &&
    prev.isHeptMuted === next.isHeptMuted &&
    prev.heptSourceKey === next.heptSourceKey &&
    prev.ipnLabel === next.ipnLabel &&
    prev.solfege === next.solfege &&
    prev.paoName === next.paoName &&
    prev.hasPerNoteCentsOverride === next.hasPerNoteCentsOverride &&
    prev.perNoteCentsValue === next.perNoteCentsValue &&
    // Compare slot by value — centsOffset is handled by slotCentsStore subscription
    prev.slot.selectedIndex === next.slot.selectedIndex &&
    prev.slot.isLocked === next.slot.isLocked &&
    prev.slot.variants === next.slot.variants
  )
}

const NoteSlider = memo(function NoteSlider({ slot, chromaticIndex, midiNote, effectiveIndex, hasOverride, isMaqamDegree, isMaqamDegreeEquiv, isMaqamTonic, isMaqamTonicEquiv, isModified, maqamVariantIndex, isWhiteKey, isHeptMuted, heptSourceKey, ipnLabel, solfege, paoName, slotCentsStore, hasPerNoteCentsOverride, perNoteCentsValue, onVariantSelect, onCentsDrag, onCentsDragEnd, onNoteCentsDrag, onNoteCentsDragEnd, onGestureStart, onGestureEnd }: Props) {
  const rootRef = useRef<HTMLDivElement>(null)
  const trackRef = useRef<HTMLDivElement>(null)
  const thumbRef = useRef<HTMLDivElement>(null)
  const dragging = useRef(false)
  const perNoteDragging = useRef(false)

  const variantCount = slot.variants.length
  // Only lock if no variants at all (empty data) — with continuous tuning, even single-variant slots are adjustable
  const isLocked     = variantCount === 0

  // Per-slot/per-note subscription: only this slider re-renders when its override changes
  const centsOverride = useSlotCentsOverride(slotCentsStore, chromaticIndex, midiNote)
  // Derive centsOffset: store override > per-note state > slot state > variant default
  const centsOffset = centsOverride ?? perNoteCentsValue ?? slot.centsOffset ?? slot.variants[slot.selectedIndex]?.midiCentsDeviation ?? 0

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

  /** Free drag on track/thumb — continuous cents update.
   *  Shift+drag = per-note override (this MIDI note only).
   *  Normal drag = chromatic slot (all octaves). */
  const handleMouseDown = (e: React.MouseEvent) => {
    if (isLocked) return
    dragging.current = true
    thumbRef.current?.classList.add('dragging')
    const isPerNote = e.shiftKey

    // Track per-note drag for violet indicator (ref survives re-renders,
    // classList gives immediate visual before any re-render)
    perNoteDragging.current = isPerNote
    if (isPerNote) rootRef.current?.classList.add('has-per-note-cents')

    // Begin DAW automation gesture (only for chromatic slot drags)
    if (!isPerNote) onGestureStart?.(chromaticIndex)

    const cents = yToCents(e.clientY)
    if (isPerNote) onNoteCentsDrag(midiNote, cents)
    else onCentsDrag(chromaticIndex, cents)

    const onMove = (ev: MouseEvent) => {
      if (!dragging.current) return
      const c = yToCents(ev.clientY)
      if (isPerNote) onNoteCentsDrag(midiNote, c)
      else onCentsDrag(chromaticIndex, c)
    }
    const onUp = (ev: MouseEvent) => {
      dragging.current = false
      perNoteDragging.current = false
      thumbRef.current?.classList.remove('dragging')
      const c = yToCents(ev.clientY)
      if (isPerNote) onNoteCentsDragEnd(midiNote, c)
      else {
        onCentsDragEnd(chromaticIndex, c)
        onGestureEnd?.(chromaticIndex)
      }
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
    <div ref={rootRef} data-midi={midiNote} className={`note-slider ${isLocked ? 'locked' : ''} ${hasOverride ? 'has-override' : ''} ${hasPerNoteCentsOverride || perNoteDragging.current ? 'has-per-note-cents' : ''} ${isMaqamDegree ? 'maqam-degree' : ''} ${isMaqamDegreeEquiv ? 'maqam-degree-equiv' : ''} ${isMaqamTonic ? 'maqam-tonic' : ''} ${isMaqamTonicEquiv ? 'maqam-tonic-equiv' : ''} ${isModified ? 'modified' : ''} ${isHeptMuted ? 'hept-muted' : ''}`}>
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
                  className={`snap-marker ${isActive ? 'active' : ''} ${i === maqamVariantIndex ? 'maqam-default' : ''}`}
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

      {/* Piano key indicator */}
      <div className={`key-bar ${isHeptMuted ? 'muted-key' : isWhiteKey ? 'white-key' : 'black-key'}`} />

      {/* IPN label — in hept mode, show source key mapping for remapped degrees */}
      <div className="ipn-label">
        {heptSourceKey
          ? <>{ipnLabel}<span className="hept-arrow">{'\u2192'}</span>{heptSourceKey}</>
          : ipnLabel}
      </div>

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
}, propsAreEqual)

export default NoteSlider
