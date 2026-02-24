import { useRef, useCallback, useState, useEffect } from 'react'
import './ReferenceFreqControl.css'

interface Props {
  cents: number
  hz: number
  defaultHz: number
  tonicChromaticIndex: number   // -1 = no maqam
  ipnNames: string[]            // 12-element array: degreeIpnMap preferred, slot ipnRef fallback
  onDrag: (cents: number) => void
  onDragEnd: (cents: number) => void
  onGestureStart: () => void
  onGestureEnd: () => void
}

const MIN_CENTS = -700
const MAX_CENTS = 700

/** Compute transposition label from cents offset relative to tonic.
 *  Always shows "X → Y" style, with +/− suffix for non-clean semitone offsets. */
function computeTransposition(
  cents: number,
  tonicIndex: number,
  ipnNames: string[]
): string | null {
  if (tonicIndex < 0) return null

  const fromIpn = ipnNames[tonicIndex] || ''
  const absCents = Math.abs(cents)
  const semitones = Math.sign(cents) * Math.round(absCents / 100)
  const toIndex = ((tonicIndex + semitones) % 12 + 12) % 12
  const toIpn = ipnNames[toIndex] || ''

  const remainder = cents - semitones * 100
  let suffix = ''
  if (remainder > 0.5) suffix = '+'
  else if (remainder < -0.5) suffix = '\u2212'  // minus sign

  if (semitones === 0) return `${fromIpn} \u2192 ${fromIpn}`
  const arrow = semitones > 0 ? '\u2197' : '\u2198'  // ↗ ↘
  return `${fromIpn} ${arrow} ${toIpn}${suffix}`
}

/** Map cents value to SVG arc angle (0 cents = 12 o'clock). */
function centsToAngle(cents: number): number {
  // -700 → -135°, 0 → 0°, +700 → +135°
  return (cents / MAX_CENTS) * 135
}

/** Draw an arc path for the knob indicator. */
function describeArc(cx: number, cy: number, r: number, startAngle: number, endAngle: number): string {
  const startRad = ((startAngle - 90) * Math.PI) / 180
  const endRad = ((endAngle - 90) * Math.PI) / 180
  const x1 = cx + r * Math.cos(startRad)
  const y1 = cy + r * Math.sin(startRad)
  const x2 = cx + r * Math.cos(endRad)
  const y2 = cy + r * Math.sin(endRad)
  const largeArc = Math.abs(endAngle - startAngle) > 180 ? 1 : 0
  const sweep = endAngle > startAngle ? 1 : 0
  return `M ${x1} ${y1} A ${r} ${r} 0 ${largeArc} ${sweep} ${x2} ${y2}`
}

export default function ReferenceFreqControl({
  cents, hz, defaultHz, tonicChromaticIndex, ipnNames,
  onDrag, onDragEnd, onGestureStart, onGestureEnd,
}: Props) {
  const isDragging = useRef(false)
  const dragStartY = useRef(0)
  const dragStartCents = useRef(0)
  const isShift = useRef(false)
  const rafRef = useRef(0)
  const pendingCents = useRef(0)
  const localCentsRef = useRef(cents)

  // Keep local ref in sync with props when not dragging
  useEffect(() => {
    if (!isDragging.current) localCentsRef.current = cents
  }, [cents])

  // Hz input state
  const [hzInputValue, setHzInputValue] = useState('')
  const [isEditingHz, setIsEditingHz] = useState(false)

  const commitHz = useCallback((inputVal: string) => {
    const parsed = parseFloat(inputVal)
    if (isNaN(parsed) || parsed <= 0 || defaultHz <= 0) return
    const newCents = 1200 * Math.log2(parsed / defaultHz)
    const clamped = Math.max(MIN_CENTS, Math.min(MAX_CENTS, newCents))
    onGestureStart()
    onDragEnd(clamped)
    onGestureEnd()
  }, [defaultHz, onDragEnd, onGestureStart, onGestureEnd])

  const handleHzFocus = useCallback(() => {
    setIsEditingHz(true)
    setHzInputValue(hz.toFixed(2))
  }, [hz])

  const handleHzBlur = useCallback(() => {
    setIsEditingHz(false)
    commitHz(hzInputValue)
  }, [hzInputValue, commitHz])

  const handleHzKeyDown = useCallback((e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === 'Enter') {
      ;(e.target as HTMLInputElement).blur()
    } else if (e.key === 'Escape') {
      setIsEditingHz(false)
    } else if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
      e.preventDefault()
      const delta = e.key === 'ArrowUp' ? 1 : -1
      const currentHz = parseFloat(hzInputValue) || hz
      const newHz = Math.max(0.01, currentHz + delta)
      setHzInputValue(newHz.toFixed(2))
      if (defaultHz > 0) {
        const newCents = 1200 * Math.log2(newHz / defaultHz)
        const clamped = Math.max(MIN_CENTS, Math.min(MAX_CENTS, newCents))
        onGestureStart()
        onDragEnd(clamped)
        onGestureEnd()
      }
    }
  }, [hzInputValue, hz, defaultHz, onDragEnd, onGestureStart, onGestureEnd])

  // ── Semitone shift buttons ────────────────────────────────────────────────

  const handleSemitoneShift = useCallback((direction: number) => {
    const newCents = Math.max(MIN_CENTS, Math.min(MAX_CENTS, cents + direction * 100))
    onGestureStart()
    onDragEnd(newCents)
    onGestureEnd()
  }, [cents, onDragEnd, onGestureStart, onGestureEnd])

  // ── Knob drag interaction ───────────────────────────────────────────────

  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    e.preventDefault()
    isDragging.current = true
    dragStartY.current = e.clientY
    dragStartCents.current = localCentsRef.current
    isShift.current = e.shiftKey
    onGestureStart()

    const handleMouseMove = (ev: MouseEvent) => {
      isShift.current = ev.shiftKey
      const dy = dragStartY.current - ev.clientY
      const sensitivity = isShift.current ? 0.3 : 2.0  // cents per pixel
      const newCents = Math.max(MIN_CENTS, Math.min(MAX_CENTS,
        dragStartCents.current + dy * sensitivity))

      localCentsRef.current = newCents
      pendingCents.current = newCents

      if (!rafRef.current) {
        rafRef.current = requestAnimationFrame(() => {
          rafRef.current = 0
          onDrag(pendingCents.current)
        })
      }
    }

    const handleMouseUp = () => {
      isDragging.current = false
      if (rafRef.current) {
        cancelAnimationFrame(rafRef.current)
        rafRef.current = 0
      }
      onDragEnd(localCentsRef.current)
      onGestureEnd()
      document.removeEventListener('mousemove', handleMouseMove)
      document.removeEventListener('mouseup', handleMouseUp)
    }

    document.addEventListener('mousemove', handleMouseMove)
    document.addEventListener('mouseup', handleMouseUp)
  }, [onDrag, onDragEnd, onGestureStart, onGestureEnd])

  const handleDoubleClick = useCallback(() => {
    onGestureStart()
    onDragEnd(0)
    onGestureEnd()
  }, [onDragEnd, onGestureStart, onGestureEnd])

  // ── SVG knob rendering ──────────────────────────────────────────────────

  const cx = 18, cy = 18, r = 14
  const angle = centsToAngle(cents)

  // Background arc (full range)
  const bgArc = describeArc(cx, cy, r, -135, 135)

  // Value arc (from 0° to current angle)
  const valueArc = angle !== 0
    ? describeArc(cx, cy, r, 0, angle)
    : ''

  // Indicator dot position
  const indicatorRad = ((angle - 90) * Math.PI) / 180
  const dotX = cx + r * Math.cos(indicatorRad)
  const dotY = cy + r * Math.sin(indicatorRad)

  // Format cents display
  const centsDisplay = cents >= 0
    ? `+${cents.toFixed(2)} ¢`
    : `${cents.toFixed(2)} ¢`

  const transposition = computeTransposition(cents, tonicChromaticIndex, ipnNames)

  return (
    <div className="ref-freq-control">
      <div className="ref-freq-label">Ref Freq</div>

      <div
        className="ref-freq-knob"
        onMouseDown={handleMouseDown}
        onDoubleClick={handleDoubleClick}
        title="Drag to adjust reference frequency. Shift+drag for fine control. Double-click to reset."
      >
        <svg width="36" height="36" viewBox="0 0 36 36">
          {/* Background arc */}
          <path d={bgArc} fill="none" stroke="var(--text-muted)" strokeWidth="3" strokeLinecap="round" opacity="0.35" />
          {/* Value arc */}
          {valueArc && (
            <path d={valueArc} fill="none" stroke="var(--accent)" strokeWidth="3.5" strokeLinecap="round" />
          )}
          {/* Center dot */}
          <circle cx={cx} cy={cy} r="3" fill="var(--surface2)" stroke="var(--text-muted)" strokeWidth="0.75" opacity="0.6" />
          {/* Indicator dot */}
          <circle cx={dotX} cy={dotY} r="3" fill="var(--accent)" />
        </svg>
      </div>

      <input
        className="ref-freq-hz-input"
        type="text"
        inputMode="decimal"
        value={isEditingHz ? hzInputValue : `${hz.toFixed(2)} Hz`}
        onChange={e => setHzInputValue(e.target.value)}
        onFocus={handleHzFocus}
        onBlur={handleHzBlur}
        onKeyDown={handleHzKeyDown}
      />

      <button
        className="ref-freq-semitone-btn"
        onClick={() => handleSemitoneShift(-1)}
        title="-100 cents (one semitone down)"
      >-</button>
      <div className="ref-freq-cents-column">
        <span className="ref-freq-cents">{centsDisplay}</span>
        <span className="ref-freq-transposition">{transposition ?? '\u00A0'}</span>
      </div>
      <button
        className="ref-freq-semitone-btn"
        onClick={() => handleSemitoneShift(+1)}
        title="+100 cents (one semitone up)"
      >+</button>
    </div>
  )
}
