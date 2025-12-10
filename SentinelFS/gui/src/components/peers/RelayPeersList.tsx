import { Globe, Server, RefreshCw } from 'lucide-react'

interface RelayPeer {
  peer_id: string
  public_endpoint: string
  connected_at: string
  capabilities?: {
    nat_type?: string
  }
}

interface RelayPeersListProps {
  relayPeers: RelayPeer[]
  isLoading: boolean
  onRefresh: () => void
}

export function RelayPeersList({ relayPeers, isLoading, onRefresh }: RelayPeersListProps) {
  if (relayPeers.length === 0) return null

  return (
    <div className="card-modern p-6">
      <div className="flex items-center gap-3 mb-4">
        <div className="relay-peers-header-icon">
          <Server className="w-5 h-5 text-accent" />
        </div>
        <div className="flex-1">
          <h3 className="font-bold text-lg">Remote Peers</h3>
          <p className="text-xs text-muted-foreground">Connected via relay server</p>
        </div>
        <button 
          onClick={onRefresh}
          disabled={isLoading}
          className="relay-peers-refresh-btn"
        >
          <RefreshCw className={`w-4 h-4 ${isLoading ? 'animate-spin' : ''}`} />
        </button>
      </div>
      
      <div className="space-y-3">
        {relayPeers.map((peer, i) => (
          <div key={i} className="relay-peer-item">
            <div className="relay-peer-icon">
              <Globe className="w-5 h-5" />
            </div>
            <div className="flex-1 min-w-0">
              <div className="font-semibold truncate">Device {peer.peer_id.substring(0, 8)}</div>
              <div className="text-xs text-muted-foreground font-mono">{peer.public_endpoint}</div>
            </div>
            <div className="text-right">
              <div className="text-xs text-muted-foreground">
                {peer.capabilities?.nat_type && (
                  <span className="relay-peer-nat-badge">
                    {peer.capabilities.nat_type}
                  </span>
                )}
              </div>
              <div className="text-xs text-muted-foreground mt-1">
                {new Date(peer.connected_at).toLocaleTimeString()}
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  )
}
