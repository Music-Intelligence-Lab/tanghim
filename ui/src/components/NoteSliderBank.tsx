import type { ChromaticSlot } from '../types'
import NoteSlider from './NoteSlider'
import './NoteSliderBank.css'

const IPN_NAMES = ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']

interface Props {
  slots: ChromaticSlot[]
  midiActiveNotes: Set<number>
  startMidi: number
  visibleCount: number
  noteNames: Record<string, Record<string, string>>
  perNoteOverrides: Record<string, number>
  maqamDegreeIndices: Set<number>
  maqamTonicIndex: number
  maqamTonicMidi: number
  onSliderChange: (chromaticIndex: number, variantIndex: number, midiNote: number, perNoteOnly: boolean) => void
}

export default function NoteSliderBank({ slots, midiActiveNotes, startMidi, visibleCount, noteNames, perNoteOverrides, maqamDegreeIndices, maqamTonicIndex, maqamTonicMidi, onSliderChange }: Props) {
  const count = Math.min(visibleCount, 128 - startMidi)

  return (
    <div className="note-slider-bank">
      {Array.from({ length: count }, (_, i) => {
        const midi = startMidi + i
        const chromaticIndex = midi % 12
        const ipnOctave = Math.floor(midi / 12) - 1
        const ipnLabel = `${IPN_NAMES[chromaticIndex]}${ipnOctave}`
        const paoName = noteNames?.[String(chromaticIndex)]?.[String(ipnOctave)] ?? '—'
        const slot = slots[chromaticIndex]

        // Resolve effective selected index: per-note override or chromatic slot default
        const noteOverride = perNoteOverrides?.[String(midi)]
        const effectiveIndex = noteOverride !== undefined ? noteOverride : slot.selectedIndex
        const hasOverride = noteOverride !== undefined

        // Home octave: tonic to tonic+11. The octave above (tonic+12) is treated as equiv.
        const isDegree = maqamDegreeIndices.has(chromaticIndex)
        const inHomeOctave = maqamTonicMidi >= 0 && midi >= maqamTonicMidi && midi < maqamTonicMidi + 12

        return (
          <NoteSlider
            key={i}
            slot={slot}
            chromaticIndex={chromaticIndex}
            midiNote={midi}
            effectiveIndex={effectiveIndex}
            hasOverride={hasOverride}
            midiActive={midiActiveNotes.has(midi)}
            isMaqamDegree={isDegree && inHomeOctave}
            isMaqamDegreeEquiv={isDegree && !inHomeOctave}
            isMaqamTonic={midi === maqamTonicMidi}
            isMaqamTonicEquiv={chromaticIndex === maqamTonicIndex && midi !== maqamTonicMidi}
            ipnLabel={ipnLabel}
            paoName={paoName}
            onVariantChange={onSliderChange}
          />
        )
      })}
    </div>
  )
}
