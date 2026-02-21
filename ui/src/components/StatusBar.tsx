import './StatusBar.css'

interface Props {
  status: string
  pluginVersion: string
  buildTimestamp: string
  onCheckForUpdates: () => void
}

export default function StatusBar({
  status,
  pluginVersion,
  buildTimestamp,
  onCheckForUpdates,
}: Props) {
  const versionDisplay = pluginVersion
    ? `v${pluginVersion} (${buildTimestamp})`
    : `(${buildTimestamp})`

  return (
    <div className="status-bar">
      <span className="version-label">{versionDisplay}</span>

      <span className="status-text">{status}</span>

      <button className="update-btn" onClick={onCheckForUpdates} title="Check for data updates">
        ↻ Updates
      </button>
    </div>
  )
}
