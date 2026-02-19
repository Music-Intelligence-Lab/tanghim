import './OutputModeSelector.css'

interface Props {
  mode: 'mts-esp' | 'mpe' | 'pitch-bend'
  isMtsTransmitter: boolean
  mtsReceivers: number
  onChange: (mode: 'mts-esp' | 'mpe' | 'pitch-bend') => void
}

const MODES = [
  { id: 'mts-esp'     as const, label: 'MTS-ESP' },
  { id: 'mpe'         as const, label: 'MPE' },
  { id: 'pitch-bend'  as const, label: 'Pitch Bend' },
]

export default function OutputModeSelector({ mode, isMtsTransmitter, mtsReceivers, onChange }: Props) {
  return (
    <div className="output-mode-selector">
      {MODES.map(m => (
        <button
          key={m.id}
          className={`mode-btn ${mode === m.id ? 'active' : ''}`}
          onClick={() => onChange(m.id)}
          title={m.id === 'mts-esp' ? (isMtsTransmitter ? `MTS-ESP transmitter · ${mtsReceivers} receivers` : 'MTS-ESP (another transmitter active)') : undefined}
        >
          {m.label}
          {m.id === 'mts-esp' && isMtsTransmitter && mtsReceivers > 0 && (
            <span className="receiver-count">{mtsReceivers}</span>
          )}
        </button>
      ))}
    </div>
  )
}
