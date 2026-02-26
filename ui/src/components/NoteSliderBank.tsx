import { memo, useMemo } from 'react'
import type { ChromaticSlot } from '../types'
import { SLOT_WIDTH_PX, BANK_LEFT_OFFSET_PX } from '../constants'
import NoteSlider from './NoteSlider'
import './NoteSliderBank.css'

const IPN_NAMES = ['C','C#','D','Eb','E','F','F#','G','Ab','A','Bb','B']
const BLACK_KEY_SET = new Set([1, 3, 6, 8, 10])

/** Compute heptatonic mapping: which positions are muted, and which white key plays each degree.
 *  Mirrors rebuildHeptMap() in C++: IPN first letter → white key assignment. */
function computeHeptInfo(
  degreeIndices: Set<number>,
  degreeIpnMap: Record<string, string>,
  slots: ChromaticSlot[]
): { muted: Set<number>; sourceKeyMap: Map<number, string> } {
  const reachable = new Set<number>()
  const sourceKeyMap = new Map<number, string>()

  // Black keys always reachable (delta 0)
  for (const bk of BLACK_KEY_SET) reachable.add(bk)

  // Degrees reachable; IPN first letter = source white key (mirrors C++ letterToWhiteIdx)
  const WHITE_KEYS: Record<string, number> = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 }
  for (const ci of degreeIndices) {
    reachable.add(ci)
    const ipn = degreeIpnMap[String(ci)] || slots[ci]?.ipnRef || ''
    if (ipn.length > 0) {
      const letter = ipn[0]
      // Only set source key if the white key position differs from the degree position
      // (i.e., the key is actually remapped, e.g. E key → Eb degree)
      if (WHITE_KEYS[letter] !== undefined && WHITE_KEYS[letter] !== ci) {
        sourceKeyMap.set(ci, letter)
      }
    }
  }

  const muted = new Set<number>()
  for (let i = 0; i < 12; i++) {
    if (!reachable.has(i)) muted.add(i)
  }
  return { muted, sourceKeyMap }
}

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
  maqamDegreePaoNames: Map<number, string>
  heptEnabled: boolean
  onVariantSelect: (chromaticIndex: number, variantIndex: number) => void
  onCentsDrag: (chromaticIndex: number, centsValue: number) => void
  onCentsDragEnd: (chromaticIndex: number, centsValue: number) => void
  onGestureStart?: (chromaticIndex: number) => void
  onGestureEnd?: (chromaticIndex: number) => void
}

const NoteSliderBank = memo(function NoteSliderBank({ slots, startMidi, bankWidthPx, noteNames, perNoteOverrides, degreeIpnMap, degreeSolfegeMap, maqamDegreeIndices, maqamTonicIndex, maqamTonicMidi, modifiedSlots, maqamDegreePaoNames, heptEnabled, onVariantSelect, onCentsDrag, onCentsDragEnd, onGestureStart, onGestureEnd }: Props) {
  const renderStart = Math.floor(startMidi)
  const pixelOffset = (startMidi - renderStart) * SLOT_WIDTH_PX

  // Render enough sliders to cover the actual pixel width plus buffer for smooth scrolling
  const fractionalCount = bankWidthPx / SLOT_WIDTH_PX
  const renderCount = Math.min(Math.ceil(fractionalCount) + 2, 128 - renderStart)

  // Hept mode: compute muted positions and source key mapping
  const heptInfo = useMemo(() =>
    heptEnabled ? computeHeptInfo(maqamDegreeIndices, degreeIpnMap, slots) : null,
    [heptEnabled, maqamDegreeIndices, degreeIpnMap, slots]
  )

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

          // Find the variant index matching the maqam's expected degree for this slot
          const expectedPaoName = maqamDegreePaoNames.get(chromaticIndex)
          const maqamVariantIndex = expectedPaoName !== undefined
            ? slot.variants.findIndex(v => v.noteName === expectedPaoName)
            : -1

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
              maqamVariantIndex={maqamVariantIndex}
              isWhiteKey={heptEnabled ? isDegree : [0,2,4,5,7,9,11].includes(chromaticIndex)}
              isHeptMuted={heptInfo?.muted.has(chromaticIndex) ?? false}
              heptSourceKey={heptInfo?.sourceKeyMap.get(chromaticIndex)}
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
