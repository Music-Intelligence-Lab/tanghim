import { useRef, useCallback, useState, useEffect } from 'react'
import './ReferenceFreqControl.css'

interface Props {
  cents: number
  hz: number
  defaultHz: number
  onDrag: (cents: number) => void
  onDragEnd: (cents: number) => void
  onGestureStart: () => void
  onGestureEnd: () => void
}

const MIN_CENTS = -700
const MAX_CENTS = 700

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
  cents, hz, defaultHz,
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
    }
  }, [])

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
      const sensitivity = isShift.current ? 0.1 : 0.5  // cents per pixel
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
          <path d={bgArc} fill="none" stroke="var(--border)" strokeWidth="3" strokeLinecap="round" />
          {/* Value arc */}
          {valueArc && (
            <path d={valueArc} fill="none" stroke="var(--accent)" strokeWidth="3" strokeLinecap="round" />
          )}
          {/* Center dot */}
          <circle cx={cx} cy={cy} r="3" fill="var(--surface)" stroke="var(--border)" strokeWidth="1" />
          {/* Indicator dot */}
          <circle cx={dotX} cy={dotY} r="2.5" fill="var(--accent)" />
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
      <span className="ref-freq-cents">{centsDisplay}</span>
      <button
        className="ref-freq-semitone-btn"
        onClick={() => handleSemitoneShift(+1)}
        title="+100 cents (one semitone up)"
      >+</button>
    </div>
  )
}
