import { Globe, Save, Plus } from 'lucide-react'
import { Section } from './Section'

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
  return (
    <div className="space-y-6">
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
    </div>
  )
}
