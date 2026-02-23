import { useRef } from 'react'
import './RangeScroller.css'

const IPN_NAMES = ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']

/** MIDI note → IPN label (e.g. 48 → "C3", 55 → "G3") */
function midiToIpn(midi: number): string {
  const oct = Math.floor(midi / 12) - 1
  return `${IPN_NAMES[midi % 12]}${oct}`
}

interface Props {
  startMidi: number
  visibleCount: number
  maqamTonicMidi: number
  onChange: (startMidi: number) => void
}

/** Pitch classes for snap ticks: C=0, G=7, A=9 */
const TICK_CLASSES = new Set([0, 7, 9])

export default function RangeScroller({ startMidi, visibleCount, maqamTonicMidi, onChange }: Props) {
  const displayStart = Math.round(startMidi)
  const endMidi = displayStart + visibleCount - 1
  const maxStart = 128 - visibleCount

  // Track the first mousedown of a double-click to detect thumb hits
  const mouseDownRef = useRef<{ x: number; thumbMidi: number; time: number } | null>(null)

  // Build tick marks at every G, A, and C within the slider range
  const ticks: { midi: number; pct: number }[] = []
  if (maxStart > 0) {
    for (let m = 0; m <= maxStart; m++) {
      if (TICK_CLASSES.has(m % 12)) {
        ticks.push({ midi: m, pct: (m / maxStart) * 100 })
      }
    }
  }

  return (
    <div className="range-scroller">
      <label>Range</label>
      <div className="range-track-wrap">
        <div className="range-ticks">
          {ticks.map(t => (
            <div
              key={t.midi}
              className="range-tick"
              style={{ left: `${t.pct}%` }}
            />
          ))}
        </div>
        <input
          type="range"
          min={0}
          max={maxStart}
          step="any"
          value={startMidi}
          onChange={e => {
            const raw = Number(e.target.value)
            // Magnetic snap: if within 1.5 MIDI notes of a tick mark (C, G, A), snap to it
            for (let m = Math.floor(raw) - 1; m <= Math.ceil(raw) + 1; m++) {
              if (m >= 0 && TICK_CLASSES.has(m % 12) && Math.abs(raw - m) < 1.1) {
                onChange(m)
                return
              }
            }
            onChange(raw)
          }}
          onMouseDown={e => {
            const now = Date.now()
            // Only record on the first mousedown — ignore the second click of a dblclick
            if (!mouseDownRef.current || now - mouseDownRef.current.time > 400) {
              mouseDownRef.current = { x: e.clientX, thumbMidi: startMidi, time: now }
            }
          }}
          onDoubleClick={e => {
            if (maqamTonicMidi < 0 || maxStart <= 0) return
            const info = mouseDownRef.current
            if (!info) return
            // Check if the first click was on the thumb (before it moved)
            const rect = e.currentTarget.getBoundingClientRect()
            const thumbHalf = 6
            const trackWidth = rect.width - thumbHalf * 2
            const thumbX = thumbHalf + (info.thumbMidi / maxStart) * trackWidth
            if (Math.abs(info.x - rect.left - thumbX) > thumbHalf + 4) return
            const padding = Math.max(0, Math.floor((visibleCount - 13) / 2))
            onChange(Math.max(0, Math.min(maxStart, maqamTonicMidi - padding)))
          }}
        />
      </div>
      <div className="range-label">
        {midiToIpn(displayStart)}–{midiToIpn(endMidi)}
      </div>
    </div>
  )
}
