import './StatusBar.css'

interface Props {
  status: string
  pluginVersion: string
  onCheckForUpdates: () => void
}

export default function StatusBar({ status, pluginVersion, onCheckForUpdates }: Props) {
  return (
    <div className="status-bar">
      {pluginVersion && <span className="version-label">v{pluginVersion}</span>}

      <span className="status-text">{status}</span>

      <button className="update-btn" onClick={onCheckForUpdates} title="Check for data updates">
        ↻ Updates
      </button>
    </div>
  )
}
