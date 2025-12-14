import { Key, Copy, Check, RefreshCw, Save, Shield, Lock } from 'lucide-react'
import { Section } from './Section'
import { Toggle } from './Toggle'

interface SecuritySettingsProps {
  config: any
  encryptionEnabled: boolean
  newSessionCode: string
  copied: boolean
  isGenerating: boolean
  onEncryptionToggle: () => void
  onSessionCodeChange: (value: string) => void
  onGenerateCode: () => void
  onCopyCode: () => void
  onSetSessionCode: () => void
}

export function SecuritySettings({
  config,
  encryptionEnabled,
  newSessionCode,
  copied,
  isGenerating,
  onEncryptionToggle,
  onSessionCodeChange,
  onGenerateCode,
  onCopyCode,
  onSetSessionCode
}: SecuritySettingsProps) {
  const formatCode = (code: string) => {
    if (!code || code.length !== 6) return code
    return `${code.slice(0, 3)}-${code.slice(3)}`
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center gap-3 mb-2">
        <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-rose-500 to-pink-600 flex items-center justify-center shadow-lg">
          <Lock className="w-5 h-5 text-white" />
        </div>
        <div>
          <h3 className="font-bold text-lg">Security Settings</h3>
          <p className="text-xs text-muted-foreground">Encryption and access control</p>
        </div>
      </div>

      <Section title="Session Code">
        {/* Current Code Display */}
        {config?.sessionCode && (
          <div className="settings-session-code-display">
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-4">
                <div className="settings-session-code-icon">
                  <Key className="w-5 h-5 text-primary" />
                </div>
                <div>
                  <div className="text-xs text-muted-foreground uppercase tracking-wider">Current Session Code</div>
                  <div className="settings-session-code-value">
                    {formatCode(config.sessionCode)}
                  </div>
                </div>
              </div>
              <button 
                onClick={onCopyCode}
                className="settings-copy-btn"
                title="Copy to clipboard"
              >
                {copied ? <Check className="w-5 h-5 icon-success" /> : <Copy className="w-5 h-5 text-muted-foreground" />}
              </button>
            </div>
            <p className="text-xs text-muted-foreground mt-3">Share this code with peers to allow them to connect.</p>
          </div>
        )}
        
        {/* No Code Warning */}
        {!config?.sessionCode && (
          <div className="settings-warning-card">
            <div className="flex items-center gap-2 status-warning mb-2">
              <Shield className="w-4 h-4" />
              <span className="text-sm font-semibold">No Session Code Set</span>
            </div>
            <p className="text-xs text-muted-foreground">Any peer can connect to your device. Set a session code for secure pairing.</p>
          </div>
        )}
        
        {/* Generate or Set Code */}
        <div className="space-y-4 mt-6">
          <div>
            <label className="settings-label">Quick Generate</label>
            <button 
              onClick={onGenerateCode}
              disabled={isGenerating}
              className="w-full settings-btn settings-btn-primary py-3"
            >
              <RefreshCw className={`w-4 h-4 ${isGenerating ? 'animate-spin' : ''}`} />
              {isGenerating ? 'Generating...' : 'Generate New Code'}
            </button>
          </div>
          
          <div className="relative">
            <div className="absolute inset-0 flex items-center">
              <div className="w-full border-t border-border/50"></div>
            </div>
            <div className="relative flex justify-center text-xs uppercase">
              <span className="bg-card px-3 text-muted-foreground">or enter manually</span>
            </div>
          </div>
          
          <div>
            <label className="settings-label">Manual Entry</label>
            <div className="flex gap-2">
              <input 
                type="text" 
                value={newSessionCode}
                onChange={(e) => onSessionCodeChange(e.target.value.toUpperCase().replace(/[^A-Z0-9]/g, ''))}
                maxLength={6}
                placeholder="ABC123"
                className="settings-input flex-1 font-mono tracking-widest text-center" 
              />
              <button 
                onClick={onSetSessionCode}
                disabled={newSessionCode.length !== 6}
                className="settings-btn settings-btn-secondary"
              >
                <Save className="w-4 h-4" />
                Set
              </button>
            </div>
          </div>
        </div>
      </Section>
      
      <Section title="Encryption">
        <div className="flex items-center justify-between mb-5">
          <div>
            <span className="text-sm font-medium block">Enable AES-256 Encryption</span>
            <span className="text-xs text-muted-foreground">Encrypt all peer-to-peer traffic</span>
          </div>
          <Toggle checked={encryptionEnabled} onChange={onEncryptionToggle} />
        </div>
        {encryptionEnabled ? (
          <div className="settings-success-card">
            <div className="flex items-center gap-2 status-success mb-2">
              <Shield className="w-4 h-4" />
              <span className="text-sm font-semibold">Encryption Active</span>
            </div>
            <p className="text-xs text-muted-foreground">All traffic is encrypted with AES-256-CBC + HMAC verification.</p>
          </div>
        ) : (
          <div className="settings-muted-card">
            <p className="text-xs text-muted-foreground">⚠ Traffic is not encrypted. Enable encryption for secure file transfers.</p>
          </div>
        )}
      </Section>
    </div>
  )
}
