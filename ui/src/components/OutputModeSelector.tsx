import './OutputModeSelector.css'

interface Props {
  isMtsTransmitter: boolean
  mtsReceivers: number
}

export default function MtsEspStatus({ isMtsTransmitter, mtsReceivers }: Props) {
  return (
    <div className="mts-esp-status">
      <span className="mts-label">MTS-ESP</span>
      {isMtsTransmitter ? (
        <span className="mts-info">
          {mtsReceivers > 0
            ? `${mtsReceivers} receiver${mtsReceivers !== 1 ? 's' : ''}`
            : 'no receivers'}
        </span>
      ) : (
        <span className="mts-info mts-warning">not connected</span>
      )}
    </div>
  )
}
