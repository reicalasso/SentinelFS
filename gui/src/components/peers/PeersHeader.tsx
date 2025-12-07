import { Network, Trash2, Plus, Scan, Loader2, Wifi } from 'lucide-react'

interface PeersHeaderProps {
  peerCount: number
  isDiscovering: boolean
  hasPeers: boolean
  onScan: () => void
  onClearPeers: () => void
  onAddRemote: () => void
}

export function PeersHeader({
  peerCount,
  isDiscovering,
  hasPeers,
  onScan,
  onClearPeers,
  onAddRemote
}: PeersHeaderProps) {
  return (
    <div className="peers-hero">
      {/* Animated Background */}
      <div className="absolute inset-0 overflow-hidden">
        <div className="peers-hero-bg-1"></div>
        <div className="peers-hero-bg-2"></div>
        <div className="peers-hero-grid-pattern"></div>
      </div>
      
      <div className="relative z-10 flex flex-col sm:flex-row sm:items-center sm:justify-between gap-4">
        <div className="flex items-center gap-3 sm:gap-4">
          <div className="peers-hero-icon">
            <Network className="w-6 h-6 sm:w-8 sm:h-8 icon-primary" />
          </div>
          <div>
            <h2 className="peers-hero-title">
              Network Mesh
            </h2>
            <p className="peers-hero-subtitle">
              <span className="dot-primary animate-pulse"></span>
              {peerCount} devices connected
            </p>
          </div>
        </div>
        
        <div className="flex flex-wrap gap-2 sm:gap-3">
          {hasPeers && (
            <button 
              onClick={onClearPeers}
              className="peers-action-btn peers-action-btn-destructive"
              title="Clear all peers"
            >
              <Trash2 className="w-4 h-4" />
            </button>
          )}
          <button 
            onClick={onAddRemote}
            className="peers-action-btn peers-action-btn-accent"
            title="Add remote peer via relay"
          >
            <Plus className="w-4 h-4" />
            <span className="hidden sm:inline">Add Remote</span>
          </button>
          <button 
            onClick={onScan}
            disabled={isDiscovering}
            className={`peers-scan-btn ${isDiscovering ? 'peers-scan-btn-disabled' : ''}`}
          >
            <div className="peers-scan-btn-shine"></div>
            <span className="relative">
              {isDiscovering ? <Loader2 className="w-4 h-4 animate-spin" /> : <Scan className="w-4 h-4" />}
            </span>
            <span className="relative hidden sm:inline">{isDiscovering ? 'Scanning...' : 'Scan for Devices'}</span>
            <span className="relative sm:hidden">{isDiscovering ? '...' : 'Scan'}</span>
          </button>
        </div>
      </div>
    </div>
  )
}

export function EmptyPeersState({ onScan }: { onScan: () => void }) {
  return (
    <div className="col-span-full card-modern p-8 sm:p-16 text-center flex flex-col items-center justify-center">
      <div className="relative mb-4 sm:mb-6">
        <div className="absolute inset-0 bg-primary/20 blur-3xl rounded-full"></div>
        <div className="relative p-4 sm:p-6 rounded-2xl sm:rounded-3xl bg-gradient-to-br from-secondary to-secondary/50 border border-border/50">
          <Wifi className="w-8 h-8 sm:w-12 sm:h-12 text-muted-foreground/40" />
        </div>
      </div>
      <h3 className="font-bold text-lg sm:text-xl text-foreground mb-2">No Devices Found</h3>
      <p className="text-sm text-muted-foreground max-w-md">
        Your mesh network is empty. Click "Scan for Devices" to automatically discover peers on your local network.
      </p>
      <button 
        onClick={onScan}
        className="mt-4 sm:mt-6 px-5 sm:px-6 py-2.5 sm:py-3 bg-primary/10 hover:bg-primary/20 text-primary rounded-xl font-medium transition-all border border-primary/20"
      >
        Start Discovery
      </button>
    </div>
  )
}
