import { X, Server, ExternalLink, Loader2, Globe } from 'lucide-react'

interface RelayModalProps {
  show: boolean
  remoteForm: {
    hostname: string
    port: string
    sessionCode: string
  }
  isConnecting: boolean
  connectionError: string | null
  onClose: () => void
  onFormChange: (form: { hostname: string; port: string; sessionCode: string }) => void
  onConnect: () => void
}

export function RelayModal({
  show,
  remoteForm,
  isConnecting,
  connectionError,
  onClose,
  onFormChange,
  onConnect
}: RelayModalProps) {
  if (!show) return null

  return (
    <div className="relay-modal-overlay">
      <div className="relay-modal">
        {/* Modal Header */}
        <div className="relay-modal-header">
          <div className="flex items-center gap-3">
            <div className="relay-modal-header-icon">
              <Server className="w-5 h-5 text-accent" />
            </div>
            <div>
              <h2 className="font-bold text-lg">Connect to Peer</h2>
              <p className="text-xs text-muted-foreground">Direct TCP connection to another SentinelFS instance</p>
            </div>
          </div>
          <button onClick={onClose} className="relay-modal-close-btn">
            <X className="w-5 h-5" />
          </button>
        </div>
        
        {/* Modal Body */}
        <div className="p-6 space-y-5">
          {/* Info Banner */}
          <div className="relay-modal-info">
            <ExternalLink className="w-5 h-5 text-accent flex-shrink-0 mt-0.5" />
            <div className="text-sm">
              <p className="font-medium text-accent mb-1">Direct Connection</p>
              <p className="text-muted-foreground text-xs leading-relaxed">
                Enter the IP address and port of another SentinelFS peer.
                Both peers must use the same session code for encrypted communication.
              </p>
            </div>
          </div>
          
          {/* Peer Address */}
          <div>
            <label className="block text-sm font-medium mb-2">Peer Address</label>
            <div className="flex gap-2">
              <input 
                type="text"
                value={remoteForm.hostname}
                onChange={e => onFormChange({ ...remoteForm, hostname: e.target.value })}
                placeholder="127.0.0.1 or 192.168.1.100"
                className="relay-modal-input flex-1"
              />
              <input 
                type="text"
                value={remoteForm.port}
                onChange={e => onFormChange({ ...remoteForm, port: e.target.value })}
                placeholder="9002"
                className="relay-modal-input w-24 text-center font-mono"
              />
            </div>
            <p className="text-xs text-muted-foreground mt-1.5">
              Default port is 8080 for main daemon, 9002+ for additional peers
            </p>
          </div>
          
          {/* Session Code - Optional info */}
          <div className="p-3 rounded-lg bg-muted/30 border border-border">
            <p className="text-xs text-muted-foreground">
              <span className="font-medium text-foreground">Note:</span> Both peers must have the same session code configured in Settings → Security for encrypted sync.
            </p>
          </div>
          
          {/* Error Message */}
          {connectionError && (
            <div className="relay-modal-error">
              {connectionError}
            </div>
          )}
        </div>
        
        {/* Modal Footer */}
        <div className="relay-modal-footer">
          <button onClick={onClose} className="relay-modal-btn-cancel">
            Cancel
          </button>
          <button 
            onClick={onConnect}
            disabled={isConnecting || !remoteForm.hostname}
            className={`relay-modal-btn-connect ${
              isConnecting || !remoteForm.hostname
                ? 'relay-modal-btn-disabled'
                : ''
            }`}
          >
            {isConnecting ? (
              <>
                <Loader2 className="w-4 h-4 animate-spin" />
                Connecting...
              </>
            ) : (
              <>
                <Globe className="w-4 h-4" />
                Connect
              </>
            )}
          </button>
        </div>
      </div>
    </div>
  )
}
