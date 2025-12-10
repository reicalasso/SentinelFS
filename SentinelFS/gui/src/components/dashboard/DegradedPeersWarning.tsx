import { AlertTriangle } from 'lucide-react'

interface DegradedPeersWarningProps {
  degradedPeers: number
}

export function DegradedPeersWarning({ degradedPeers }: DegradedPeersWarningProps) {
  if (degradedPeers <= 0) {
    return null
  }

  return (
    <div className="degraded-peers-warning">
      <AlertTriangle className="w-5 h-5 icon-warning" />
      <div>
        <div className="degraded-peers-title">
          {degradedPeers} Degraded Peer{degradedPeers > 1 ? 's' : ''}
        </div>
        <div className="degraded-peers-message">
          High jitter or packet loss detected. Check network conditions.
        </div>
      </div>
    </div>
  )
}
