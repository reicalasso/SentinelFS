import { useState, useEffect, memo } from 'react'
import { 
  PeerCard, 
  DiscoveryPanel, 
  RelayModal, 
  RelayPeersList, 
  PeersHeader, 
  EmptyPeersState 
} from './peers'

interface RemotePeerForm {
  hostname: string
  port: string
  sessionCode: string
}

interface RelayPeer {
  peer_id: string
  public_endpoint: string
  connected_at: string
  capabilities?: {
    nat_type?: string
  }
}

export const Peers = memo(function Peers({ peers }: { peers?: any[] }) {
  const [isDiscovering, setIsDiscovering] = useState(false)
  const [discoverySettings, setDiscoverySettings] = useState({ udp: true, tcp: false })
  const [relayStatus, setRelayStatus] = useState({ enabled: false, connected: false })
  const [latencyHistory, setLatencyHistory] = useState<Record<string, {time: string, latency: number}[]>>({})
  const [blockedPeers, setBlockedPeers] = useState<Set<string>>(new Set())
  
  // Remote peer modal state
  const [showAddRemote, setShowAddRemote] = useState(false)
  const [remoteForm, setRemoteForm] = useState<RemotePeerForm>({ hostname: '', port: '9000', sessionCode: '' })
  const [isConnecting, setIsConnecting] = useState(false)
  const [connectionError, setConnectionError] = useState<string | null>(null)
  
  // Relay peers from remote server
  const [relayPeers, setRelayPeers] = useState<RelayPeer[]>([])
  const [isLoadingRelayPeers, setIsLoadingRelayPeers] = useState(false)
  
  // Local session code - synced between localStorage and daemon
  const [localSessionCode, setLocalSessionCode] = useState<string>(() => {
    // Initialize from localStorage
    return localStorage.getItem('sentinelfs_session_code') || ''
  })
  
  // Fetch session code from daemon on mount and sync
  useEffect(() => {
    const fetchAndSyncSessionCode = async () => {
      if (window.api) {
        try {
          const result = await window.api.sendCommand('NETFALCON_STATUS') as { success: boolean; output?: string }
          if (result.success && result.output) {
            const status = JSON.parse(result.output)
            if (status.sessionCode) {
              // Daemon has a code - use it and save to localStorage
              setLocalSessionCode(status.sessionCode)
              localStorage.setItem('sentinelfs_session_code', status.sessionCode)
            } else {
              // Daemon has no code - check if we have one in localStorage to send
              const savedCode = localStorage.getItem('sentinelfs_session_code')
              if (savedCode) {
                await window.api.sendCommand(`SET_SESSION_CODE ${savedCode}`)
                setLocalSessionCode(savedCode)
              }
            }
          }
        } catch {}
      }
    }
    fetchAndSyncSessionCode()
    
    // Also listen for config updates
    const handleConfigUpdate = (event: CustomEvent) => {
      if (event.detail?.sessionCode) {
        setLocalSessionCode(event.detail.sessionCode)
        localStorage.setItem('sentinelfs_session_code', event.detail.sessionCode)
      }
    }
    window.addEventListener('config-updated' as any, handleConfigUpdate)
    return () => window.removeEventListener('config-updated' as any, handleConfigUpdate)
  }, [])
  
  // Load discovery settings from localStorage on mount
  useEffect(() => {
    const saved = localStorage.getItem('discoverySettings')
    if (saved) {
      try {
        const parsed = JSON.parse(saved)
        setDiscoverySettings(parsed)
      } catch {}
    }
  }, [])
  
  // Track latency history for each peer
  useEffect(() => {
    if (!peers || peers.length === 0) return
    
    const now = new Date()
    const timeStr = `${now.getMinutes()}:${now.getSeconds().toString().padStart(2, '0')}`
    
    setLatencyHistory(prev => {
      const updated = { ...prev }
      peers.forEach(peer => {
        if (peer.id && peer.latency >= 0) {
          const history = updated[peer.id] || []
          updated[peer.id] = [...history.slice(-20), { time: timeStr, latency: peer.latency }]
        }
      })
      return updated
    })
  }, [peers])

  const handleScan = async () => {
    if (window.api) {
      setIsDiscovering(true)
      await window.api.sendCommand('DISCOVER')
      setTimeout(() => setIsDiscovering(false), 3000)
    }
  }

  const toggleSetting = async (key: 'udp' | 'tcp') => {
    const newSettings = { ...discoverySettings, [key]: !discoverySettings[key] }
    setDiscoverySettings(newSettings)
    localStorage.setItem('discoverySettings', JSON.stringify(newSettings))
    
    if (window.api) {
      const cmd = key === 'udp' 
        ? `SET_DISCOVERY udp=${newSettings.udp ? '1' : '0'}`
        : `SET_DISCOVERY tcp=${newSettings.tcp ? '1' : '0'}`
      await window.api.sendCommand(cmd)
    }
  }
  
  const handleBlockPeer = async (peerId: string) => {
    if (window.api && peerId) {
      await window.api.sendCommand(`BLOCK_PEER ${peerId}`)
      setBlockedPeers(prev => new Set([...prev, peerId]))
    }
  }
  
  const handleClearPeers = async () => {
    if (window.api && confirm('Clear all peers from database? They will be re-discovered on next scan.')) {
      await window.api.sendCommand('CLEAR_PEERS')
      setLatencyHistory({})
    }
  }
  
  const handleConnectRemote = async () => {
    if (!remoteForm.hostname) {
      setConnectionError('Hostname/IP is required')
      return
    }
    
    setIsConnecting(true)
    setConnectionError(null)
    
    try {
      const host = remoteForm.hostname
      const port = parseInt(remoteForm.port) || 9002
      
      if (window.api) {
        // Use ADD_PEER for direct TCP connection
        const result = await window.api.sendCommand(`ADD_PEER ${host}:${port}`) as { success: boolean; output?: string; error?: string }
        if (!result.success || result.output?.includes('Error')) {
          throw new Error(result.output || result.error || 'Connection failed')
        }
      }
      
      setShowAddRemote(false)
      setRemoteForm({ hostname: '', port: '9002', sessionCode: '' })
    } catch (err) {
      setConnectionError(err instanceof Error ? err.message : 'Connection failed')
    } finally {
      setIsConnecting(false)
    }
  }
  
  const fetchRelayPeers = async (host: string, port: number, sessionCode: string) => {
    setIsLoadingRelayPeers(true)
    try {
      const response = await fetch(`http://${host}:${port}/peers?session=${encodeURIComponent(sessionCode)}`)
      if (response.ok) {
        const data = await response.json()
        setRelayPeers(data.peers || [])
      }
    } catch (err) {
      console.error('Failed to fetch relay peers:', err)
    } finally {
      setIsLoadingRelayPeers(false)
    }
  }
  
  const regenerateSessionCode = async () => {
    // Generate 6-character code (same format as SecuritySettings)
    const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
    const code = Array.from({ length: 6 }, () => chars[Math.floor(Math.random() * chars.length)]).join('')
    
    // Save to localStorage immediately
    localStorage.setItem('sentinelfs_session_code', code)
    setLocalSessionCode(code)
    
    // Send to daemon
    if (window.api) {
      await window.api.sendCommand(`SET_SESSION_CODE ${code}`)
      // Dispatch event so Settings page can update too
      window.dispatchEvent(new CustomEvent('config-updated', { detail: { sessionCode: code } }))
    }
  }
  
  const handleRefreshRelayPeers = () => {
    const config = localStorage.getItem('sentinelfs_relay_config')
    if (config) {
      const { host, port, sessionCode } = JSON.parse(config)
      fetchRelayPeers(host, port, sessionCode)
    }
  }

  // Filter out blocked peers and map backend peers to UI format
  const displayPeers = (peers && peers.length > 0) ? peers
    .filter(p => !blockedPeers.has(p.id))
    .map(p => ({
      id: p.id,
      name: p.id ? `Device ${p.id.substring(0, 8)}` : 'Unknown Device',
      ip: `${p.ip || 'Unknown'}:${p.port || '?'}`,
      type: 'laptop',
      status: p.status || 'Disconnected',
      trusted: true,
      latency: p.latency >= 0 ? p.latency : -1,
      lastSeen: p.latency >= 0 ? `${p.latency}ms` : 'Unknown',
      transport: p.transport || 'QUIC' as 'TCP' | 'QUIC' | 'WebRTC' | 'Relay'
    })) : []

  return (
    <div className="space-y-4 sm:space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
      <PeersHeader
        peerCount={displayPeers.length}
        isDiscovering={isDiscovering}
        hasPeers={displayPeers.length > 0}
        onScan={handleScan}
        onClearPeers={handleClearPeers}
        onAddRemote={() => setShowAddRemote(true)}
      />

      <div className="grid grid-cols-1 sm:grid-cols-2 xl:grid-cols-3 gap-4 sm:gap-6">
        {displayPeers.length === 0 && (
          <EmptyPeersState onScan={handleScan} />
        )}
        {displayPeers.map((peer, i) => (
          <PeerCard
            key={i}
            peer={peer}
            index={i}
            latencyHistory={latencyHistory[peer.id]}
            onBlock={handleBlockPeer}
          />
        ))}
      </div>

      <DiscoveryPanel
        discoverySettings={discoverySettings}
        relayStatus={relayStatus}
        localSessionCode={localSessionCode}
        onToggleSetting={toggleSetting}
        onRegenerateCode={regenerateSessionCode}
      />
      
      <RelayPeersList
        relayPeers={relayPeers}
        isLoading={isLoadingRelayPeers}
        onRefresh={handleRefreshRelayPeers}
      />
      
      <RelayModal
        show={showAddRemote}
        remoteForm={remoteForm}
        isConnecting={isConnecting}
        connectionError={connectionError}
        onClose={() => {
          setShowAddRemote(false)
          setConnectionError(null)
        }}
        onFormChange={setRemoteForm}
        onConnect={handleConnectRemote}
      />
    </div>
  )
})
