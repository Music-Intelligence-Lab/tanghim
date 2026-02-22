import './OutputModeSelector.css'

interface Props {
  isMtsTransmitter: boolean
  oscillatorEnabled: boolean
  mtsNativeCount: number
  mpeCount: number
  monoPbCount: number
  onOscillatorToggle: () => void
}

export default function MtsEspStatus({ isMtsTransmitter, oscillatorEnabled, mtsNativeCount, mpeCount, monoPbCount, onOscillatorToggle }: Props) {
  const active = isMtsTransmitter

  return (
    <div className="mts-esp-status">
      <span className={`mts-badge mts-badge-osc${oscillatorEnabled ? '' : ' mts-badge-inactive'}`}
            onClick={onOscillatorToggle}
            title="Internal reference oscillator">
        Osc
      </span>
      <span className={`mts-badge mts-badge-native${active && mtsNativeCount > 0 ? '' : ' mts-badge-inactive'}`}>
        MTS-ESP<span className="mts-badge-count">{active ? mtsNativeCount : 0}</span>
      </span>
      <span className={`mts-badge mts-badge-mpe${active && mpeCount > 0 ? '' : ' mts-badge-inactive'}`}>
        MPE<span className="mts-badge-count">{active ? mpeCount : 0}</span>
      </span>
      <span className={`mts-badge mts-badge-mono${active && monoPbCount > 0 ? '' : ' mts-badge-inactive'}`}>
        Mono PB<span className="mts-badge-count">{active ? monoPbCount : 0}</span>
      </span>
    </div>
  )
}
