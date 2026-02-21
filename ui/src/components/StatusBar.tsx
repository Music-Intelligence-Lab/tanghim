import './StatusBar.css'

interface Props {
  status: string
  pluginVersion: string
  onCheckForUpdates: () => void
}

export default function StatusBar({ status, pluginVersion, onCheckForUpdates }: Props) {
  const versionDisplay = pluginVersion
    ? `v${pluginVersion} (${__BUILD_TIMESTAMP__})`
    : `(${__BUILD_TIMESTAMP__})`

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
