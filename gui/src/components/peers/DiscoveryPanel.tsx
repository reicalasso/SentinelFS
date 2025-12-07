import { Wifi, Globe, Shield, Key, Copy, Check, RefreshCw } from 'lucide-react'
import { useState } from 'react'

interface DiscoveryPanelProps {
  discoverySettings: { udp: boolean; tcp: boolean }
  relayStatus: { enabled: boolean; connected: boolean }
  localSessionCode: string
  onToggleSetting: (key: 'udp' | 'tcp') => void
  onRegenerateCode: () => void
}

export function DiscoveryPanel({
  discoverySettings,
  relayStatus,
  localSessionCode,
  onToggleSetting,
  onRegenerateCode
}: DiscoveryPanelProps) {
  const [sessionCodeCopied, setSessionCodeCopied] = useState(false)

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
      <div className="flex items-center gap-2 sm:gap-3 mb-4 sm:mb-6">
        <div className="discovery-panel-icon">
          <Shield className="w-4 h-4 sm:w-5 sm:h-5 text-primary" />
        </div>
        <div>
          <h3 className="font-bold text-base sm:text-lg">Discovery Settings</h3>
          <p className="text-xs text-muted-foreground">Configure how devices find each other</p>
        </div>
      </div>
      
      <div className="grid grid-cols-1 md:grid-cols-2 gap-3 sm:gap-4">
        {/* Local Network Discovery */}
        <div className="discovery-toggle-card group">
          <div className="discovery-toggle-bg"></div>
          
          <div className="relative">
            <div className="font-semibold flex items-center gap-2">
              <Wifi className="w-4 h-4 text-primary" />
              Local Network Discovery
            </div>
            <div className="text-xs text-muted-foreground mt-1.5">
              Automatically find peers on the same Wi-Fi
            </div>
          </div>
          <div 
            className={`discovery-toggle ${discoverySettings.udp ? 'discovery-toggle-on' : 'discovery-toggle-off'}`}
            onClick={() => onToggleSetting('udp')}
          >
            <div className={`discovery-toggle-knob ${discoverySettings.udp ? 'translate-x-7' : 'translate-x-1'}`}></div>
          </div>
        </div>
        
        {/* Global Relay */}
        <div className="discovery-toggle-card discovery-toggle-card-accent group">
          <div className="discovery-toggle-bg discovery-toggle-bg-accent"></div>
          
          <div className="relative">
            <div className="font-semibold flex items-center gap-2">
              <Globe className="w-4 h-4 text-accent" />
              Global Relay
              {discoverySettings.tcp && (
                <span className={`discovery-relay-status ${
                  relayStatus.connected ? 'discovery-relay-status-connected' : 'discovery-relay-status-connecting'
                }`}>
                  {relayStatus.connected ? 'Connected' : 'Connecting...'}
                </span>
              )}
            </div>
            <div className="text-xs text-muted-foreground mt-1.5">
              Connect via relay when direct connection fails
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
      
      {/* Your Session Code */}
      <div className="session-code-panel">
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center gap-2">
            <Key className="w-4 h-4 text-violet-400" />
            <span className="font-semibold text-sm">Your Session Code</span>
          </div>
          <button 
            onClick={onRegenerateCode}
            className="text-xs text-muted-foreground hover:text-foreground transition-colors flex items-center gap-1"
          >
            <RefreshCw className="w-3 h-3" /> Regenerate
          </button>
        </div>
        <div className="flex items-center gap-3">
          <code className="session-code-display">
            {localSessionCode}
          </code>
          <button 
            onClick={copySessionCode}
            className={`session-code-copy-btn ${sessionCodeCopied ? 'session-code-copy-btn-success' : ''}`}
          >
            {sessionCodeCopied ? <Check className="w-5 h-5" /> : <Copy className="w-5 h-5" />}
          </button>
        </div>
        <p className="text-xs text-muted-foreground mt-2">
          Share this code with other devices to connect via relay server
        </p>
      </div>
    </div>
  )
}
