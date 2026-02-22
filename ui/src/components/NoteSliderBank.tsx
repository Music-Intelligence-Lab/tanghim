import { memo } from 'react'
import type { ChromaticSlot } from '../types'
import { SLOT_WIDTH_PX, BANK_LEFT_OFFSET_PX } from '../constants'
import NoteSlider from './NoteSlider'
import './NoteSliderBank.css'

const IPN_NAMES = ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']

interface Props {
  slots: ChromaticSlot[]
  startMidi: number
  bankWidthPx: number
  noteNames: Record<string, Record<string, string>>
  perNoteOverrides: Record<string, number>
  degreeIpnMap: Record<string, string>
  degreeSolfegeMap: Record<string, string>
  maqamDegreeIndices: Set<number>
  maqamTonicIndex: number
  maqamTonicMidi: number
  modifiedSlots: Set<number>
  onVariantSelect: (chromaticIndex: number, variantIndex: number) => void
  onCentsDrag: (chromaticIndex: number, centsValue: number) => void
  onCentsDragEnd: (chromaticIndex: number, centsValue: number) => void
  onGestureStart?: (chromaticIndex: number) => void
  onGestureEnd?: (chromaticIndex: number) => void
}

const NoteSliderBank = memo(function NoteSliderBank({ slots, startMidi, bankWidthPx, noteNames, perNoteOverrides, degreeIpnMap, degreeSolfegeMap, maqamDegreeIndices, maqamTonicIndex, maqamTonicMidi, modifiedSlots, onVariantSelect, onCentsDrag, onCentsDragEnd, onGestureStart, onGestureEnd }: Props) {
  const renderStart = Math.floor(startMidi)
  const pixelOffset = (startMidi - renderStart) * SLOT_WIDTH_PX

  // Render enough sliders to cover the actual pixel width plus buffer for smooth scrolling
  const fractionalCount = bankWidthPx / SLOT_WIDTH_PX
  const renderCount = Math.min(Math.ceil(fractionalCount) + 2, 128 - renderStart)

  return (
    <div className="note-slider-bank">
      <div className="note-slider-bank-inner" style={{ transform: `translateX(${BANK_LEFT_OFFSET_PX - pixelOffset}px)` }}>
        {Array.from({ length: renderCount }, (_, i) => {
          const midi = renderStart + i
          const chromaticIndex = midi % 12
          const ipnOctave = Math.floor(midi / 12) - 1
          const ipnName = degreeIpnMap?.[String(chromaticIndex)] ?? IPN_NAMES[chromaticIndex]
          const ipnLabel = `${ipnName}${ipnOctave}`
          const paoName = noteNames?.[String(chromaticIndex)]?.[String(ipnOctave)] ?? '—'
          const slot = slots[chromaticIndex]

          // Resolve effective selected index: per-note override or chromatic slot default
          const noteOverride = perNoteOverrides?.[String(midi)]
          const effectiveIndex = noteOverride !== undefined ? noteOverride : slot.selectedIndex
          const hasOverride = noteOverride !== undefined

          // Get solfege: prefer maqam detail source (same as IPN), fall back to variant
          const solfege = degreeSolfegeMap?.[String(chromaticIndex)] ?? slot.variants[effectiveIndex]?.solfege ?? '—'

          // Home octave: tonic to tonic+11. The octave above (tonic+12) is treated as equiv.
          const isDegree = maqamDegreeIndices.has(chromaticIndex)
          const inHomeOctave = maqamTonicMidi >= 0 && midi >= maqamTonicMidi && midi < maqamTonicMidi + 12
          const isModified = modifiedSlots.has(chromaticIndex)

          return (
            <NoteSlider
              key={midi}
              slot={slot}
              chromaticIndex={chromaticIndex}
              midiNote={midi}
              effectiveIndex={effectiveIndex}
              hasOverride={hasOverride}
              isMaqamDegree={isDegree && inHomeOctave}
              isMaqamDegreeEquiv={isDegree && !inHomeOctave}
              isMaqamTonic={midi === maqamTonicMidi}
              isMaqamTonicEquiv={chromaticIndex === maqamTonicIndex && midi !== maqamTonicMidi}
              isModified={isModified}
              ipnLabel={ipnLabel}
              solfege={solfege}
              paoName={paoName}
              onVariantSelect={onVariantSelect}
              onCentsDrag={onCentsDrag}
              onCentsDragEnd={onCentsDragEnd}
              onGestureStart={onGestureStart}
              onGestureEnd={onGestureEnd}
            />
          )
        })}
      </div>
    </div>
  )
})

export default NoteSliderBank
