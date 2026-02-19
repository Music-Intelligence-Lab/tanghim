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
  const endMidi = startMidi + visibleCount - 1
  const maxStart = 128 - visibleCount

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
      <div
        className="range-track-wrap"
        onDoubleClick={() => {
          if (maqamTonicMidi >= 0) onChange(Math.max(0, Math.min(maxStart, maqamTonicMidi)))
        }}
      >
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
          value={startMidi}
          onChange={e => onChange(Number(e.target.value))}
        />
      </div>
      <div className="range-label">
        {midiToIpn(startMidi)}–{midiToIpn(endMidi)}
      </div>
    </div>
  )
}
