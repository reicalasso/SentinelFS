import { Globe, Save, Plus, Zap, Network, Shield, Activity, ChevronDown } from 'lucide-react'
import { Section } from './Section'
import { useState, useEffect } from 'react'

interface NetworkSettingsProps {
  peerIp: string
  peerPort: string
  uploadLimit: string
  downloadLimit: string
  onPeerIpChange: (value: string) => void
  onPeerPortChange: (value: string) => void
  onUploadLimitChange: (value: string) => void
  onDownloadLimitChange: (value: string) => void
  onAddPeer: () => void
  onApplyUploadLimit: () => void
  onApplyDownloadLimit: () => void
}

type TransportStrategy = 'FALLBACK_CHAIN' | 'PREFER_FAST' | 'PREFER_RELIABLE' | 'ADAPTIVE'

const strategyDescriptions: Record<TransportStrategy, string> = {
  'ADAPTIVE': 'Akıllı seçim: NAT, gecikme ve dosya boyutuna göre',
  'PREFER_FAST': 'En düşük gecikme (QUIC öncelikli)',
  'PREFER_RELIABLE': 'En güvenilir bağlantı (TCP öncelikli)',
  'FALLBACK_CHAIN': 'Sırayla dene: QUIC → TCP → WebRTC → Relay'
}

const transportInfo: Record<string, { description: string; useCase: string; color: string }> = {
  tcp: {
    description: 'Reliable, Guaranteed delivery',
    useCase: 'Large files, reliable networks',
    color: 'emerald'
  },
  quic: {
    description: 'Low latency, 0-RTT connection',
    useCase: 'Default, fast networks',
    color: 'orange'
  },
  relay: {
    description: 'Bridge for NAT/Firewall',
    useCase: 'Firewall/Symmetric NAT',
    color: 'violet'
  },
  webrtc: {
    description: 'Browser-based P2P',
    useCase: 'Web interface, NAT traversal internal',
    color: 'cyan'
  }
}

const STORAGE_KEY_STRATEGY = 'netfalcon_strategy'
const STORAGE_KEY_TRANSPORTS = 'netfalcon_transports'
const STORAGE_KEY_ADVANCED = 'netfalcon_advanced'

export function NetworkSettings({
  peerIp,
  peerPort,
  uploadLimit,
  downloadLimit,
  onPeerIpChange,
  onPeerPortChange,
  onUploadLimitChange,
  onDownloadLimitChange,
  onAddPeer,
  onApplyUploadLimit,
  onApplyDownloadLimit
}: NetworkSettingsProps) {
  // Load from localStorage on mount
  const [transportStrategy, setTransportStrategy] = useState<TransportStrategy>(() => {
    const saved = localStorage.getItem(STORAGE_KEY_STRATEGY)
    return (saved as TransportStrategy) || 'FALLBACK_CHAIN'
  })
  
  const [transports, setTransports] = useState(() => {
    const saved = localStorage.getItem(STORAGE_KEY_TRANSPORTS)
    if (saved) {
      try {
        return JSON.parse(saved)
      } catch {}
    }
    return { quic: true, tcp: true, webrtc: false, relay: true }
  })
  
  const [showAdvanced, setShowAdvanced] = useState(false)
  
  const [advancedSettings, setAdvancedSettings] = useState(() => {
    const saved = localStorage.getItem(STORAGE_KEY_ADVANCED)
    if (saved) {
      try {
        return JSON.parse(saved)
      } catch {}
    }
    return { maxConnections: 64, reconnectDelay: 5000, autoReconnect: true, keyRotation: true }
  })

  // Sync settings to daemon on mount
  useEffect(() => {
    const syncSettings = async () => {
      if (window.api) {
        await window.api.sendCommand(`NETFALCON_SET_STRATEGY ${transportStrategy}`)
        await window.api.sendCommand(`NETFALCON_SET_TRANSPORT tcp=${transports.tcp ? '1' : '0'}`)
        await window.api.sendCommand(`NETFALCON_SET_TRANSPORT quic=${transports.quic ? '1' : '0'}`)
        await window.api.sendCommand(`NETFALCON_SET_TRANSPORT relay=${transports.relay ? '1' : '0'}`)
      }
    }
    syncSettings()
  }, []) // Only on mount

  const handleStrategyChange = async (strategy: TransportStrategy) => {
    setTransportStrategy(strategy)
    localStorage.setItem(STORAGE_KEY_STRATEGY, strategy)
    // Dispatch custom event for same-window updates
    window.dispatchEvent(new CustomEvent('netfalcon-settings-changed', { detail: { strategy } }))
    if (window.api) {
      await window.api.sendCommand(`NETFALCON_SET_STRATEGY ${strategy}`)
    }
  }

  const toggleTransport = async (transport: 'tcp' | 'quic' | 'relay' | 'webrtc') => {
    const newTransports = { ...transports, [transport]: !transports[transport] }
    setTransports(newTransports)
    localStorage.setItem(STORAGE_KEY_TRANSPORTS, JSON.stringify(newTransports))
    // Dispatch custom event for same-window updates
    window.dispatchEvent(new CustomEvent('netfalcon-settings-changed', { detail: { transports: newTransports } }))
    if (window.api) {
      await window.api.sendCommand(`NETFALCON_SET_TRANSPORT ${transport}=${newTransports[transport] ? '1' : '0'}`)
    }
  }
  
  const updateAdvancedSetting = (key: string, value: any) => {
    const newSettings = { ...advancedSettings, [key]: value }
    setAdvancedSettings(newSettings)
    localStorage.setItem(STORAGE_KEY_ADVANCED, JSON.stringify(newSettings))
  }

  return (
    <div className="space-y-6">
      {/* NetFalcon Branding */}
      <div className="flex items-center gap-3 mb-2">
        <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-violet-500 to-indigo-600 flex items-center justify-center shadow-lg">
          <Zap className="w-5 h-5 text-white" />
        </div>
        <div>
          <h3 className="font-bold text-lg flex items-center gap-2">
            NetFalcon
            <span className="text-xs px-2 py-0.5 rounded-full bg-violet-500/20 text-violet-400 font-medium">v1.0</span>
          </h3>
          <p className="text-xs text-muted-foreground">Multi-transport network engine</p>
        </div>
      </div>

      {/* Transport Strategy */}
      <Section title="Transport Strategy">
        <div className="grid grid-cols-2 gap-2">
          {(Object.keys(strategyDescriptions) as TransportStrategy[]).map((strategy) => (
            <button
              key={strategy}
              onClick={() => handleStrategyChange(strategy)}
              className={`p-3 rounded-lg border text-left transition-all ${
                transportStrategy === strategy
                  ? 'border-violet-500 bg-violet-500/10 ring-1 ring-violet-500/50'
                  : 'border-border hover:border-violet-500/50 hover:bg-violet-500/5'
              }`}
            >
              <div className="font-medium text-sm flex items-center gap-2">
                {strategy === 'ADAPTIVE' && <Activity className="w-4 h-4 text-violet-400" />}
                {strategy === 'PREFER_FAST' && <Zap className="w-4 h-4 text-amber-400" />}
                {strategy === 'PREFER_RELIABLE' && <Shield className="w-4 h-4 text-emerald-400" />}
                {strategy === 'FALLBACK_CHAIN' && <Network className="w-4 h-4 text-blue-400" />}
                {strategy.replace('_', ' ')}
              </div>
              <p className="text-xs text-muted-foreground mt-1">{strategyDescriptions[strategy]}</p>
            </button>
          ))}
        </div>
      </Section>

      {/* Active Transports */}
      <Section title="Transport Protokolleri">
        <div className="space-y-3">
          {/* QUIC - Varsayılan */}
          <div 
            onClick={() => toggleTransport('quic')}
            className={`p-4 rounded-xl border cursor-pointer transition-all ${
              transports.quic
                ? 'border-orange-500 bg-orange-500/10 ring-1 ring-orange-500/30'
                : 'border-border hover:border-orange-500/50'
            }`}
          >
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className={`w-3 h-3 rounded-full ${transports.quic ? 'bg-orange-400' : 'bg-muted'}`} />
                <div>
                  <div className="font-semibold flex items-center gap-2">
                    QUIC
                    <span className="text-[10px] px-1.5 py-0.5 rounded bg-orange-500/30 text-orange-300">Varsayılan</span>
                  </div>
                  <p className="text-xs text-muted-foreground">{transportInfo.quic.description}</p>
                </div>
              </div>
              <div className="text-xs text-right text-muted-foreground">
                <div className="font-medium text-orange-400">Hızlı ağlar</div>
                <div>0-RTT bağlantı</div>
              </div>
            </div>
          </div>

          {/* TCP */}
          <div 
            onClick={() => toggleTransport('tcp')}
            className={`p-4 rounded-xl border cursor-pointer transition-all ${
              transports.tcp
                ? 'border-emerald-500 bg-emerald-500/10 ring-1 ring-emerald-500/30'
                : 'border-border hover:border-emerald-500/50'
            }`}
          >
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className={`w-3 h-3 rounded-full ${transports.tcp ? 'bg-emerald-500' : 'bg-muted'}`} />
                <div>
                  <div className="font-semibold">TCP</div>
                  <p className="text-xs text-muted-foreground">{transportInfo.tcp.description}</p>
                </div>
              </div>
              <div className="text-xs text-right text-muted-foreground">
                <div className="font-medium text-emerald-400">Büyük dosyalar</div>
                <div>Garanti teslimat</div>
              </div>
            </div>
          </div>

          {/* WebRTC */}
          <div 
            onClick={() => toggleTransport('webrtc')}
            className={`p-4 rounded-xl border cursor-pointer transition-all ${
              transports.webrtc
                ? 'border-cyan-500 bg-cyan-500/10 ring-1 ring-cyan-500/30'
                : 'border-border hover:border-cyan-500/50'
            }`}
          >
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className={`w-3 h-3 rounded-full ${transports.webrtc ? 'bg-cyan-400' : 'bg-muted'}`} />
                <div>
                  <div className="font-semibold flex items-center gap-2">
                    WebRTC
                    <span className="text-[10px] px-1.5 py-0.5 rounded bg-cyan-500/30 text-cyan-300">P2P</span>
                  </div>
                  <p className="text-xs text-muted-foreground">{transportInfo.webrtc.description}</p>
                </div>
              </div>
              <div className="text-xs text-right text-muted-foreground">
                <div className="font-medium text-cyan-400">Tarayıcı erişimi</div>
                <div>NAT traversal dahili</div>
              </div>
            </div>
          </div>

          {/* Relay */}
          <div 
            onClick={() => toggleTransport('relay')}
            className={`p-4 rounded-xl border cursor-pointer transition-all ${
              transports.relay
                ? 'border-violet-500 bg-violet-500/10 ring-1 ring-violet-500/30'
                : 'border-border hover:border-violet-500/50'
            }`}
          >
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className={`w-3 h-3 rounded-full ${transports.relay ? 'bg-violet-500' : 'bg-muted'}`} />
                <div>
                  <div className="font-semibold flex items-center gap-2">
                    Relay
                    <span className="text-[10px] px-1.5 py-0.5 rounded bg-violet-500/30 text-violet-300">Fallback</span>
                  </div>
                  <p className="text-xs text-muted-foreground">{transportInfo.relay.description}</p>
                </div>
              </div>
              <div className="text-xs text-right text-muted-foreground">
                <div className="font-medium text-violet-400">Her zaman çalışır</div>
                <div>Firewall/Symmetric NAT</div>
              </div>
            </div>
          </div>
        </div>
        <p className="text-xs text-muted-foreground mt-3">
          NetFalcon seçilen stratejiye göre en uygun transport'u otomatik seçer
        </p>
      </Section>

      {/* Manual Peer Connection */}
      <Section title="Manual Peer Connection">
        <div className="settings-info-card settings-info-card-primary">
          <div className="flex items-start gap-2 sm:gap-3 mb-3 sm:mb-4">
            <div className="settings-info-icon">
              <Globe className="w-4 h-4 text-primary" />
            </div>
            <div>
              <span className="text-xs sm:text-sm font-semibold">Connect to a specific peer</span>
              <p className="text-xs text-muted-foreground mt-0.5 sm:mt-1">
                Enter the IP address and port of a peer on a different network
              </p>
            </div>
          </div>
          <div className="grid grid-cols-1 sm:grid-cols-[1fr_auto_auto] gap-2">
            <input 
              type="text" 
              value={peerIp}
              onChange={(e) => onPeerIpChange(e.target.value)}
              placeholder="192.168.1.100"
              className="settings-input" 
            />
            <input 
              type="number" 
              value={peerPort}
              onChange={(e) => onPeerPortChange(e.target.value)}
              placeholder="8080"
              className="settings-input w-full sm:w-24 text-center" 
            />
            <button onClick={onAddPeer} className="settings-btn settings-btn-primary">
              <Plus className="w-4 h-4" />
              Connect
            </button>
          </div>
        </div>
      </Section>
      
      {/* Bandwidth Limits */}
      <Section title="Bandwidth Limits">
        <div className="mb-5">
          <label className="settings-label">Upload Limit (KB/s)</label>
          <div className="flex gap-2">
            <input 
              type="number" 
              value={uploadLimit}
              onChange={(e) => onUploadLimitChange(e.target.value)}
              placeholder="0 = Unlimited"
              className="settings-input flex-1" 
            />
            <button onClick={onApplyUploadLimit} className="settings-btn settings-btn-primary">
              <Save className="w-4 h-4" />
              Apply
            </button>
          </div>
          <div className="text-xs text-muted-foreground mt-1.5">0 = Unlimited</div>
        </div>
        <div className="mb-4">
          <label className="settings-label">Download Limit (KB/s)</label>
          <div className="flex gap-2">
            <input 
              type="number" 
              value={downloadLimit}
              onChange={(e) => onDownloadLimitChange(e.target.value)}
              placeholder="0 = Unlimited"
              className="settings-input flex-1" 
            />
            <button onClick={onApplyDownloadLimit} className="settings-btn settings-btn-primary">
              <Save className="w-4 h-4" />
              Apply
            </button>
          </div>
          <div className="text-xs text-muted-foreground mt-1.5">0 = Unlimited</div>
        </div>
      </Section>

      {/* Advanced Settings Toggle */}
      <button
        onClick={() => setShowAdvanced(!showAdvanced)}
        className="w-full flex items-center justify-between p-3 rounded-lg border border-border hover:bg-muted/50 transition-colors"
      >
        <span className="text-sm font-medium">Advanced NetFalcon Settings</span>
        <ChevronDown className={`w-4 h-4 transition-transform ${showAdvanced ? 'rotate-180' : ''}`} />
      </button>

      {showAdvanced && (
        <div className="space-y-4 p-4 rounded-lg bg-muted/30 border border-border">
          <div className="grid grid-cols-2 gap-4">
            <div>
              <label className="settings-label">Max Connections</label>
              <input 
                type="number" 
                value={advancedSettings.maxConnections}
                onChange={(e) => updateAdvancedSetting('maxConnections', parseInt(e.target.value) || 64)}
                className="settings-input" 
              />
            </div>
            <div>
              <label className="settings-label">Reconnect Delay (ms)</label>
              <input 
                type="number" 
                value={advancedSettings.reconnectDelay}
                onChange={(e) => updateAdvancedSetting('reconnectDelay', parseInt(e.target.value) || 5000)}
                className="settings-input" 
              />
            </div>
          </div>
          <div className="flex items-center justify-between">
            <div>
              <div className="font-medium text-sm">Auto Reconnect</div>
              <p className="text-xs text-muted-foreground">Automatically reconnect on connection loss</p>
            </div>
            <div 
              onClick={() => updateAdvancedSetting('autoReconnect', !advancedSettings.autoReconnect)}
              className={`w-12 h-6 rounded-full relative cursor-pointer transition-colors ${
                advancedSettings.autoReconnect ? 'bg-violet-500' : 'bg-muted'
              }`}
            >
              <div className={`absolute w-5 h-5 bg-white rounded-full top-0.5 shadow transition-transform ${
                advancedSettings.autoReconnect ? 'right-0.5' : 'left-0.5'
              }`} />
            </div>
          </div>
          <div className="flex items-center justify-between">
            <div>
              <div className="font-medium text-sm">Key Rotation</div>
              <p className="text-xs text-muted-foreground">Periodically rotate encryption keys</p>
            </div>
            <div 
              onClick={() => updateAdvancedSetting('keyRotation', !advancedSettings.keyRotation)}
              className={`w-12 h-6 rounded-full relative cursor-pointer transition-colors ${
                advancedSettings.keyRotation ? 'bg-violet-500' : 'bg-muted'
              }`}
            >
              <div className={`absolute w-5 h-5 bg-white rounded-full top-0.5 shadow transition-transform ${
                advancedSettings.keyRotation ? 'right-0.5' : 'left-0.5'
              }`} />
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
