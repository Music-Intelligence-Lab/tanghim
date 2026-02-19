import './OutputModeSelector.css'

interface Props {
  isMtsTransmitter: boolean
  mtsNativeCount: number
  mpeCount: number
  monoPbCount: number
}

export default function MtsEspStatus({ isMtsTransmitter, mtsNativeCount, mpeCount, monoPbCount }: Props) {
  if (!isMtsTransmitter) {
    return (
      <div className="mts-esp-status">
        <span className="mts-label">MTS-ESP</span>
        <span className="mts-info mts-warning">not connected</span>
      </div>
    )
  }

  const total = mtsNativeCount + mpeCount + monoPbCount

  if (total === 0) {
    return (
      <div className="mts-esp-status">
        <span className="mts-label">MTS-ESP</span>
        <span className="mts-info">no receivers</span>
      </div>
    )
  }

  return (
    <div className="mts-esp-status">
      {mtsNativeCount > 0 && (
        <span className="mts-badge mts-badge-native">
          MTS-ESP<span className="mts-badge-count">{mtsNativeCount}</span>
        </span>
      )}
      {mpeCount > 0 && (
        <span className="mts-badge mts-badge-mpe">
          MPE<span className="mts-badge-count">{mpeCount}</span>
        </span>
      )}
      {monoPbCount > 0 && (
        <span className="mts-badge mts-badge-mono">
          Mono PB<span className="mts-badge-count">{monoPbCount}</span>
        </span>
      )}
    </div>
  )
}
