import { Wifi, Globe, Shield, Key, Copy, Check, RefreshCw, Zap, Activity, Network } from 'lucide-react'
import { useState, useEffect, useCallback } from 'react'

interface DiscoveryPanelProps {
  discoverySettings: { udp: boolean; tcp: boolean }
  relayStatus: { enabled: boolean; connected: boolean }
  localSessionCode: string
  onToggleSetting: (key: 'udp' | 'tcp') => void
  onRegenerateCode: () => void
}

interface TransportStatus {
  tcp: { enabled: boolean; listening: boolean }
  quic: { enabled: boolean; listening: boolean }
  relay: { enabled: boolean; connected: boolean }
  webrtc: { enabled: boolean; ready: boolean }
}

interface NetFalconStatus {
  strategy: string
  transports: {
    tcp: { enabled: boolean; listening: boolean }
    quic: { enabled: boolean; listening: boolean }
    relay: { enabled: boolean; connected: boolean }
  }
}

export function DiscoveryPanel({
  discoverySettings,
  relayStatus,
  localSessionCode,
  onToggleSetting,
  onRegenerateCode
}: DiscoveryPanelProps) {
  const [sessionCodeCopied, setSessionCodeCopied] = useState(false)
  const [transportStatus, setTransportStatus] = useState<TransportStatus>({
    tcp: { enabled: true, listening: false },
    quic: { enabled: true, listening: false },
    relay: { enabled: true, connected: false },
    webrtc: { enabled: false, ready: false }
  })
  const [strategy, setStrategy] = useState('FALLBACK_CHAIN')

  // Fetch transport status from daemon and localStorage
  const fetchStatus = useCallback(async () => {
    // First, read from localStorage for immediate UI update
    const savedTransports = localStorage.getItem('netfalcon_transports')
    const savedStrategy = localStorage.getItem('netfalcon_strategy')
    
    if (savedTransports) {
      try {
        const parsed = JSON.parse(savedTransports)
        setTransportStatus(prev => ({
          quic: { ...prev.quic, enabled: parsed.quic ?? true },
          tcp: { ...prev.tcp, enabled: parsed.tcp ?? true },
          webrtc: { ...prev.webrtc, enabled: parsed.webrtc ?? false },
          relay: { ...prev.relay, enabled: parsed.relay ?? true }
        }))
      } catch {}
    }
    
    if (savedStrategy) {
      setStrategy(savedStrategy)
    }
    
    // Then fetch from daemon for accurate state
    if (window.api) {
      try {
        const result = await window.api.sendCommand('NETFALCON_STATUS') as { success: boolean; output?: string; error?: string }
        if (result.success && result.output) {
          try {
            const status: NetFalconStatus = JSON.parse(result.output)
            setTransportStatus(prev => ({
              tcp: status.transports?.tcp || { enabled: true, listening: false },
              quic: status.transports?.quic || { enabled: false, listening: false },
              relay: status.transports?.relay || { enabled: false, connected: false },
              webrtc: prev.webrtc // Keep webrtc from local state
            }))
            setStrategy(status.strategy || 'FALLBACK_CHAIN')
          } catch {}
        }
      } catch {}
    }
  }, [])

  // Fetch on mount and periodically
  useEffect(() => {
    fetchStatus()
    const interval = setInterval(fetchStatus, 2000)
    return () => clearInterval(interval)
  }, [fetchStatus])

  // Listen for storage changes (from Settings page in different tab)
  useEffect(() => {
    const handleStorageChange = (e: StorageEvent) => {
      if (e.key === 'netfalcon_transports' || e.key === 'netfalcon_strategy') {
        fetchStatus()
      }
    }
    window.addEventListener('storage', handleStorageChange)
    return () => window.removeEventListener('storage', handleStorageChange)
  }, [fetchStatus])

  // Listen for custom event (from Settings page in same window)
  useEffect(() => {
    const handleSettingsChanged = (e: Event) => {
      const customEvent = e as CustomEvent
      if (customEvent.detail?.transports) {
        const t = customEvent.detail.transports
        setTransportStatus(prev => ({
          tcp: { ...prev.tcp, enabled: t.tcp ?? prev.tcp.enabled },
          quic: { ...prev.quic, enabled: t.quic ?? prev.quic.enabled },
          relay: { ...prev.relay, enabled: t.relay ?? prev.relay.enabled },
          webrtc: { ...prev.webrtc, enabled: t.webrtc ?? prev.webrtc.enabled }
        }))
      }
      if (customEvent.detail?.strategy) {
        setStrategy(customEvent.detail.strategy)
      }
    }
    window.addEventListener('netfalcon-settings-changed', handleSettingsChanged)
    return () => window.removeEventListener('netfalcon-settings-changed', handleSettingsChanged)
  }, [])

  // Also fetch when discoverySettings prop changes
  useEffect(() => {
    fetchStatus()
  }, [discoverySettings, fetchStatus])

  const copySessionCode = async () => {
    try {
      await navigator.clipboard.writeText(localSessionCode)
      setSessionCodeCopied(true)
      setTimeout(() => setSessionCodeCopied(false), 2000)
    } catch (err) {
      console.error('Failed to copy:', err)
    }
  }

  return (
    <div className="card-modern p-4 sm:p-6 mt-6 sm:mt-8">
      {/* NetFalcon Header */}
      <div className="flex items-center justify-between mb-4 sm:mb-6">
        <div className="flex items-center gap-2 sm:gap-3">
          <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-violet-500 to-indigo-600 flex items-center justify-center shadow-lg">
            <Zap className="w-5 h-5 text-white" />
          </div>
          <div>
            <h3 className="font-bold text-base sm:text-lg flex items-center gap-2">
              NetFalcon Discovery
              <span className="text-[10px] px-1.5 py-0.5 rounded-full bg-emerald-500/20 text-emerald-400 font-medium">Active</span>
            </h3>
            <p className="text-xs text-muted-foreground">Multi-transport peer discovery</p>
          </div>
        </div>
        
        {/* Transport Status Indicators - Priority Order: QUIC > TCP > WebRTC > Relay */}
        <div className="hidden sm:flex items-center gap-2">
          <div className={`flex items-center gap-1.5 px-2 py-1 rounded-full text-xs font-medium ${
            transportStatus.quic.enabled ? 'bg-orange-500/20 text-orange-300' : 'bg-muted text-muted-foreground'
          }`}>
            <div className={`w-1.5 h-1.5 rounded-full ${transportStatus.quic.enabled ? 'bg-orange-400 animate-pulse' : 'bg-muted-foreground'}`} />
            QUIC
          </div>
          <div className={`flex items-center gap-1.5 px-2 py-1 rounded-full text-xs font-medium ${
            transportStatus.tcp.enabled ? 'bg-emerald-500/20 text-emerald-400' : 'bg-muted text-muted-foreground'
          }`}>
            <div className={`w-1.5 h-1.5 rounded-full ${transportStatus.tcp.enabled ? 'bg-emerald-500 animate-pulse' : 'bg-muted-foreground'}`} />
            TCP
          </div>
          <div className={`flex items-center gap-1.5 px-2 py-1 rounded-full text-xs font-medium ${
            transportStatus.webrtc.enabled ? 'bg-cyan-500/20 text-cyan-400' : 'bg-muted text-muted-foreground'
          }`}>
            <div className={`w-1.5 h-1.5 rounded-full ${transportStatus.webrtc.enabled ? 'bg-cyan-400 animate-pulse' : 'bg-muted-foreground'}`} />
            WebRTC
          </div>
          <div className={`flex items-center gap-1.5 px-2 py-1 rounded-full text-xs font-medium ${
            transportStatus.relay.enabled ? 'bg-violet-500/20 text-violet-400' : 'bg-muted text-muted-foreground'
          }`}>
            <div className={`w-1.5 h-1.5 rounded-full ${transportStatus.relay.enabled ? 'bg-violet-500 animate-pulse' : 'bg-muted-foreground'}`} />
            Relay
          </div>
        </div>
      </div>
      
      <div className="grid grid-cols-1 md:grid-cols-2 gap-3 sm:gap-4">
        {/* Local Network Discovery */}
        <div className="discovery-toggle-card group">
          <div className="discovery-toggle-bg"></div>
          
          <div className="relative">
            <div className="font-semibold flex items-center gap-2">
              <Wifi className="w-4 h-4 text-primary" />
              UDP Broadcast
              <span className="text-[10px] px-1.5 py-0.5 rounded bg-primary/20 text-primary">LAN</span>
            </div>
            <div className="text-xs text-muted-foreground mt-1.5">
              Discover peers on local network via UDP broadcast
            </div>
          </div>
          <div 
            className={`discovery-toggle ${discoverySettings.udp ? 'discovery-toggle-on' : 'discovery-toggle-off'}`}
            onClick={() => onToggleSetting('udp')}
          >
            <div className={`discovery-toggle-knob ${discoverySettings.udp ? 'translate-x-7' : 'translate-x-1'}`}></div>
          </div>
        </div>
        
        {/* Relay Transport */}
        <div className="discovery-toggle-card discovery-toggle-card-accent group">
          <div className="discovery-toggle-bg discovery-toggle-bg-accent"></div>
          
          <div className="relative">
            <div className="font-semibold flex items-center gap-2">
              <Globe className="w-4 h-4 text-accent" />
              Relay Transport
              {discoverySettings.tcp && (
                <span className={`discovery-relay-status ${
                  relayStatus.connected ? 'discovery-relay-status-connected' : 'discovery-relay-status-connecting'
                }`}>
                  {relayStatus.connected ? 'Connected' : 'Connecting...'}
                </span>
              )}
            </div>
            <div className="text-xs text-muted-foreground mt-1.5">
              NAT traversal via relay server (fallback)
            </div>
          </div>
          <div 
            className={`discovery-toggle ${discoverySettings.tcp ? 'discovery-toggle-on-accent' : 'discovery-toggle-off'}`}
            onClick={() => onToggleSetting('tcp')}
          >
            <div className={`discovery-toggle-knob ${discoverySettings.tcp ? 'translate-x-7' : 'translate-x-1'}`}></div>
          </div>
        </div>
      </div>

      {/* Session Code Section */}
      <div className="mt-4 p-3 rounded-lg bg-gradient-to-r from-primary/10 to-accent/10 border border-primary/20">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-2">
            <Key className="w-4 h-4 text-primary" />
            <span className="text-sm font-medium">Session Code</span>
          </div>
          <div className="flex items-center gap-2">
            {localSessionCode ? (
              <>
                <code className="px-3 py-1.5 rounded-lg bg-background/50 border border-border font-mono text-sm tracking-widest">
                  {localSessionCode.length === 6 
                    ? `${localSessionCode.slice(0, 3)}-${localSessionCode.slice(3)}`
                    : localSessionCode}
                </code>
                <button
                  onClick={copySessionCode}
                  className="p-2 rounded-lg hover:bg-background/50 transition-colors"
                  title="Copy session code"
                >
                  {sessionCodeCopied ? (
                    <Check className="w-4 h-4 text-emerald-400" />
                  ) : (
                    <Copy className="w-4 h-4 text-muted-foreground" />
                  )}
                </button>
              </>
            ) : (
              <span className="text-xs text-amber-400 px-2 py-1 rounded bg-amber-500/10">Not Set</span>
            )}
            <button
              onClick={onRegenerateCode}
              className="p-2 rounded-lg hover:bg-background/50 transition-colors"
              title={localSessionCode ? "Generate new code" : "Generate code"}
            >
              <RefreshCw className="w-4 h-4 text-muted-foreground" />
            </button>
          </div>
        </div>
        <p className="text-[11px] text-muted-foreground mt-2">
          {localSessionCode 
            ? "Share this code with peers to enable secure connections"
            : "Click refresh to generate a session code for secure peer connections"}
        </p>
      </div>

      {/* Connection Quality Indicator */}
      <div className="mt-4 p-3 rounded-lg bg-muted/30 border border-border">
        <div className="flex items-center justify-between mb-2">
          <div className="flex items-center gap-2 text-sm font-medium">
            <Activity className="w-4 h-4 text-violet-400" />
            Transport Selection
          </div>
          <span className="text-xs text-muted-foreground">Strategy: {strategy.replace('_', ' ')}</span>
        </div>
        {/* Priority Order: QUIC > TCP > WebRTC > Relay */}
        <div className="flex items-center gap-4 text-xs flex-wrap">
          {transportStatus.quic.enabled && (
            <div className="flex items-center gap-1.5">
              <Zap className="w-3.5 h-3.5 text-orange-400" />
              <span className="font-medium text-orange-300">QUIC</span>
              <span className="text-[10px] px-1 py-0.5 rounded bg-orange-500/20 text-orange-300">Varsayılan</span>
              {transportStatus.quic.listening && <span className="text-muted-foreground">(listening)</span>}
            </div>
          )}
          {transportStatus.tcp.enabled && (
            <div className="flex items-center gap-1.5">
              <Network className="w-3.5 h-3.5 text-emerald-400" />
              <span className="font-medium text-emerald-400">TCP</span>
              {transportStatus.tcp.listening && <span className="text-muted-foreground">(listening)</span>}
            </div>
          )}
          {transportStatus.webrtc.enabled && (
            <div className="flex items-center gap-1.5">
              <Globe className="w-3.5 h-3.5 text-cyan-400" />
              <span className="font-medium text-cyan-400">WebRTC</span>
              {transportStatus.webrtc.ready && <span className="text-emerald-400">(ready)</span>}
            </div>
          )}
          {transportStatus.relay.enabled && (
            <div className="flex items-center gap-1.5">
              <Shield className="w-3.5 h-3.5 text-violet-400" />
              <span className="font-medium text-violet-400">Relay</span>
              <span className="text-[10px] px-1 py-0.5 rounded bg-violet-500/20 text-violet-300">Fallback</span>
              {transportStatus.relay.connected && <span className="text-emerald-400">(connected)</span>}
            </div>
          )}
          {!transportStatus.tcp.enabled && !transportStatus.quic.enabled && !transportStatus.relay.enabled && !transportStatus.webrtc.enabled && (
            <span className="text-muted-foreground">No transports enabled</span>
          )}
        </div>
      </div>
    </div>
  )
}
