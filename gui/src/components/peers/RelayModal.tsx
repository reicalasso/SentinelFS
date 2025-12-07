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
              <h2 className="font-bold text-lg">Connect to Relay Server</h2>
              <p className="text-xs text-muted-foreground">Join a remote peer network</p>
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
              <p className="font-medium text-accent mb-1">How it works</p>
              <p className="text-muted-foreground text-xs leading-relaxed">
                Enter the relay server address and the session code shared by your peer. 
                Both devices must use the same session code to discover each other.
              </p>
            </div>
          </div>
          
          {/* Server Address */}
          <div>
            <label className="block text-sm font-medium mb-2">Relay Server Address</label>
            <div className="flex gap-2">
              <input 
                type="text"
                value={remoteForm.hostname}
                onChange={e => onFormChange({ ...remoteForm, hostname: e.target.value })}
                placeholder="relay.example.com"
                className="relay-modal-input flex-1"
              />
              <input 
                type="text"
                value={remoteForm.port}
                onChange={e => onFormChange({ ...remoteForm, port: e.target.value })}
                placeholder="9000"
                className="relay-modal-input w-24 text-center font-mono"
              />
            </div>
            <p className="text-xs text-muted-foreground mt-1.5">
              Default port is 9000 for SentinelFS relay servers
            </p>
          </div>
          
          {/* Session Code */}
          <div>
            <label className="block text-sm font-medium mb-2">Session Code</label>
            <input 
              type="text"
              value={remoteForm.sessionCode}
              onChange={e => onFormChange({ ...remoteForm, sessionCode: e.target.value.toUpperCase() })}
              placeholder="Enter session code from peer"
              className="relay-modal-input w-full font-mono tracking-wider uppercase"
              maxLength={32}
            />
            <p className="text-xs text-muted-foreground mt-1.5">
              Get this code from the device you want to connect to
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
            disabled={isConnecting || !remoteForm.hostname || !remoteForm.sessionCode}
            className={`relay-modal-btn-connect ${
              isConnecting || !remoteForm.hostname || !remoteForm.sessionCode
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
