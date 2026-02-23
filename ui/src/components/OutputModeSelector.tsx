import './OutputModeSelector.css'

interface Props {
  isMtsTransmitter: boolean
  oscillatorEnabled: boolean
  heptEnabled: boolean
  hasMaqam: boolean
  mtsNativeCount: number
  mpeCount: number
  monoPbCount: number
  onOscillatorToggle: () => void
  onHeptToggle: () => void
}

export default function MtsEspStatus({ isMtsTransmitter, oscillatorEnabled, heptEnabled, hasMaqam, mtsNativeCount, mpeCount, monoPbCount, onOscillatorToggle, onHeptToggle }: Props) {
  const active = isMtsTransmitter

  return (
    <div className="mts-esp-status">
      <div className="mts-row">
        <span className={`mts-badge mts-badge-osc${oscillatorEnabled ? '' : ' mts-badge-inactive'}`}
              onClick={onOscillatorToggle}
              title="Internal reference oscillator">
          Osc
        </span>
        <span className={`mts-badge mts-badge-hept${heptEnabled && hasMaqam ? '' : ' mts-badge-inactive'}`}
              onClick={onHeptToggle}
              title="Heptatonic keyboard mapping">
          Hept
        </span>
      </div>
      <div className="mts-row">
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
    </div>
  )
}
