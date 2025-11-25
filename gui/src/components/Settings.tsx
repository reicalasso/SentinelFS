import { useState, useEffect } from 'react'
import { Bell, Database, Globe, Lock, Moon, Shield, Smartphone, Save } from 'lucide-react'

interface SettingsProps {
  config: any
}

export function Settings({ config }: SettingsProps) {
  const [activeTab, setActiveTab] = useState('general')
  
  // Local state for form inputs
  const [uploadLimit, setUploadLimit] = useState('0')
  const [downloadLimit, setDownloadLimit] = useState('0')
  const [sessionCode, setSessionCode] = useState('')
  const [encryptionEnabled, setEncryptionEnabled] = useState(false)
  const [syncEnabled, setSyncEnabled] = useState(true)
  
  // Update local state when config changes
  useEffect(() => {
    if (config) {
      setUploadLimit(config.uploadLimit?.toString() || '0')
      setDownloadLimit(config.downloadLimit?.toString() || '0')
      setEncryptionEnabled(config.encryption || false)
      setSyncEnabled(config.syncEnabled !== false)
    }
  }, [config])
  
  const sendConfig = async (key: string, value: string) => {
    if (window.api) {
      await window.api.sendCommand(`SET_CONFIG ${key}=${value}`)
    }
  }
  
  const handleUploadLimitChange = () => {
    sendConfig('uploadLimit', uploadLimit)
  }
  
  const handleDownloadLimitChange = () => {
    sendConfig('downloadLimit', downloadLimit)
  }
  
  const handleSessionCodeChange = () => {
    if (sessionCode.length === 6) {
      sendConfig('sessionCode', sessionCode)
      setSessionCode('') // Clear after setting
    }
  }
  
  const handleEncryptionToggle = () => {
    const newValue = !encryptionEnabled
    setEncryptionEnabled(newValue)
    sendConfig('encryption', newValue ? 'true' : 'false')
  }
  
  const handleSyncToggle = () => {
    const newValue = !syncEnabled
    setSyncEnabled(newValue)
    sendConfig('syncEnabled', newValue ? 'true' : 'false')
  }

  return (
    <div className="max-w-4xl mx-auto animate-in fade-in duration-500">
      <h2 className="text-lg font-semibold mb-6">Settings</h2>
      
      <div className="grid grid-cols-1 md:grid-cols-4 gap-8">
        {/* Settings Navigation */}
        <nav className="space-y-1">
            <SettingsTab 
                active={activeTab === 'general'} 
                onClick={() => setActiveTab('general')} 
                icon={<Smartphone className="w-4 h-4" />} 
                label="General" 
            />
            <SettingsTab 
                active={activeTab === 'network'} 
                onClick={() => setActiveTab('network')} 
                icon={<Globe className="w-4 h-4" />} 
                label="Network" 
            />
            <SettingsTab 
                active={activeTab === 'security'} 
                onClick={() => setActiveTab('security')} 
                icon={<Lock className="w-4 h-4" />} 
                label="Security" 
            />
            <SettingsTab 
                active={activeTab === 'advanced'} 
                onClick={() => setActiveTab('advanced')} 
                icon={<Database className="w-4 h-4" />} 
                label="Advanced" 
            />
        </nav>

        {/* Settings Content */}
        <div className="md:col-span-3 space-y-6">
            {activeTab === 'general' && (
                <div className="space-y-6">
                    <Section title="Synchronization">
                        <div className="flex items-center justify-between">
                            <div>
                                <span className="text-sm font-medium block">Enable Sync</span>
                                <span className="text-xs text-muted-foreground">Pause/resume file synchronization</span>
                            </div>
                            <Toggle checked={syncEnabled} onChange={handleSyncToggle} />
                        </div>
                    </Section>
                    <Section title="Configuration">
                        <div className="space-y-2 text-xs">
                            <div className="flex justify-between py-1">
                                <span className="text-muted-foreground">TCP Port:</span>
                                <span className="font-mono">{config?.tcpPort || 8080}</span>
                            </div>
                            <div className="flex justify-between py-1">
                                <span className="text-muted-foreground">Discovery Port:</span>
                                <span className="font-mono">{config?.discoveryPort || 9999}</span>
                            </div>
                            <div className="flex justify-between py-1">
                                <span className="text-muted-foreground">Watch Directory:</span>
                                <span className="font-mono text-xs">{config?.watchDirectory || '~/sentinel_sync'}</span>
                            </div>
                        </div>
                    </Section>
                </div>
            )}

            {activeTab === 'network' && (
                <div className="space-y-6">
                    <Section title="Bandwidth Limits">
                        <div className="mb-4">
                            <label className="block text-xs font-medium text-muted-foreground mb-1.5 uppercase tracking-wider">Upload Limit (KB/s)</label>
                            <div className="flex gap-2">
                                <input 
                                    type="number" 
                                    value={uploadLimit}
                                    onChange={(e) => setUploadLimit(e.target.value)}
                                    placeholder="0 = Unlimited"
                                    className="flex-1 bg-background border border-input rounded-md px-3 py-2 text-sm focus:ring-1 focus:ring-blue-500 outline-none transition-all" 
                                />
                                <button 
                                    onClick={handleUploadLimitChange}
                                    className="px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-md text-sm font-medium transition-colors flex items-center gap-2"
                                >
                                    <Save className="w-4 h-4" />
                                    Apply
                                </button>
                            </div>
                            <div className="text-xs text-muted-foreground mt-1">0 = Unlimited</div>
                        </div>
                        <div className="mb-4">
                            <label className="block text-xs font-medium text-muted-foreground mb-1.5 uppercase tracking-wider">Download Limit (KB/s)</label>
                            <div className="flex gap-2">
                                <input 
                                    type="number" 
                                    value={downloadLimit}
                                    onChange={(e) => setDownloadLimit(e.target.value)}
                                    placeholder="0 = Unlimited"
                                    className="flex-1 bg-background border border-input rounded-md px-3 py-2 text-sm focus:ring-1 focus:ring-blue-500 outline-none transition-all" 
                                />
                                <button 
                                    onClick={handleDownloadLimitChange}
                                    className="px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-md text-sm font-medium transition-colors flex items-center gap-2"
                                >
                                    <Save className="w-4 h-4" />
                                    Apply
                                </button>
                            </div>
                            <div className="text-xs text-muted-foreground mt-1">0 = Unlimited</div>
                        </div>
                    </Section>
                </div>
            )}
            
            {activeTab === 'security' && (
                <div className="space-y-6">
                    <Section title="Session Code">
                        <div className="mb-4">
                            <label className="block text-xs font-medium text-muted-foreground mb-1.5 uppercase tracking-wider">6-Character Code</label>
                            <div className="flex gap-2">
                                <input 
                                    type="text" 
                                    value={sessionCode}
                                    onChange={(e) => setSessionCode(e.target.value.toUpperCase())}
                                    maxLength={6}
                                    placeholder="ABC123"
                                    className="flex-1 bg-background border border-input rounded-md px-3 py-2 text-sm font-mono focus:ring-1 focus:ring-blue-500 outline-none transition-all" 
                                />
                                <button 
                                    onClick={handleSessionCodeChange}
                                    disabled={sessionCode.length !== 6}
                                    className="px-4 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-secondary disabled:text-muted-foreground text-white rounded-md text-sm font-medium transition-colors flex items-center gap-2"
                                >
                                    <Save className="w-4 h-4" />
                                    Set
                                </button>
                            </div>
                            <div className="text-xs text-muted-foreground mt-1">
                                {config?.hasSessionCode ? '✓ Session code is set' : '⚠ No session code set - any peer can connect'}
                            </div>
                        </div>
                    </Section>
                    <Section title="Encryption">
                        <div className="flex items-center justify-between mb-4">
                            <div>
                                <span className="text-sm font-medium block">Enable AES-256 Encryption</span>
                                <span className="text-xs text-muted-foreground">Requires session code to be set</span>
                            </div>
                            <Toggle checked={encryptionEnabled} onChange={handleEncryptionToggle} />
                        </div>
                        {encryptionEnabled && (
                            <div className="bg-green-500/10 p-4 rounded-lg border border-green-500/20">
                                <div className="flex items-center gap-2 text-green-500 mb-2">
                                    <Shield className="w-4 h-4" />
                                    <span className="text-sm font-semibold">Encryption Active</span>
                                </div>
                                <p className="text-xs text-muted-foreground">All traffic is encrypted with AES-256-CBC</p>
                            </div>
                        )}
                    </Section>
                </div>
            )}

            {activeTab === 'advanced' && (
                <div className="space-y-6">
                    <Section title="System Information">
                        <div className="space-y-2 text-xs">
                            <div className="flex justify-between py-1">
                                <span className="text-muted-foreground">Metrics Port:</span>
                                <span className="font-mono">{config?.metricsPort || 9100}</span>
                            </div>
                            <div className="flex justify-between py-1">
                                <span className="text-muted-foreground">Database:</span>
                                <span className="font-mono">sentinel.db (SQLite)</span>
                            </div>
                            <div className="flex justify-between py-1">
                                <span className="text-muted-foreground">IPC Socket:</span>
                                <span className="font-mono text-xs">/run/user/1000/sentinelfs/sentinel_daemon.sock</span>
                            </div>
                        </div>
                    </Section>
                    <Section title="Delta Sync Engine">
                        <div className="space-y-2 text-xs">
                            <div className="flex justify-between py-1">
                                <span className="text-muted-foreground">Algorithm:</span>
                                <span className="font-mono">Rolling Checksum (Adler32) + SHA-256</span>
                            </div>
                            <div className="flex justify-between py-1">
                                <span className="text-muted-foreground">Block Size:</span>
                                <span className="font-mono">4 KB</span>
                            </div>
                        </div>
                    </Section>
                    <Section title="ML Anomaly Detection">
                        <div className="bg-yellow-500/10 p-4 rounded-lg border border-yellow-500/20">
                            <div className="flex items-center gap-2 text-yellow-500 mb-2">
                                <Database className="w-4 h-4" />
                                <span className="text-sm font-semibold">Experimental Feature</span>
                            </div>
                            <p className="text-xs text-muted-foreground">ML-based anomaly detection uses IsolationForest via ONNX Runtime to identify suspicious peer behavior. This feature is in beta.</p>
                        </div>
                    </Section>
                    <Section title="Danger Zone">
                        <div className="space-y-4">
                            <button 
                                onClick={() => window.api?.sendCommand('STATUS')}
                                className="w-full px-4 py-2 bg-secondary hover:bg-secondary/80 text-foreground rounded-md text-sm font-medium transition-colors"
                            >
                                Refresh Daemon Status
                            </button>
                            <button 
                                className="w-full px-4 py-2 bg-red-500/10 hover:bg-red-500/20 text-red-500 border border-red-500/20 rounded-md text-sm font-medium transition-colors"
                                onClick={() => {
                                    if (confirm('Are you sure you want to reset all settings to defaults?')) {
                                        // Reset to defaults
                                        sendConfig('uploadLimit', '0')
                                        sendConfig('downloadLimit', '0')
                                    }
                                }}
                            >
                                Reset to Defaults
                            </button>
                        </div>
                    </Section>
                </div>
            )}
        </div>
      </div>
    </div>
  )
}

function SettingsTab({ active, onClick, icon, label }: any) {
    return (
        <button 
            onClick={onClick}
            className={`w-full flex items-center gap-3 px-4 py-2 rounded-lg text-sm font-medium transition-colors ${
                active ? 'bg-secondary text-foreground' : 'text-muted-foreground hover:bg-secondary/50 hover:text-foreground'
            }`}
        >
            {icon}
            {label}
        </button>
    )
}

function Section({ title, children }: any) {
    return (
        <div className="bg-card border border-border rounded-xl p-6 shadow-sm">
            <h3 className="font-medium mb-4 pb-2 border-b border-border">{title}</h3>
            {children}
        </div>
    )
}

function Input({ label, value }: any) {
    return (
        <div className="mb-4 last:mb-0">
            <label className="block text-xs font-medium text-muted-foreground mb-1.5 uppercase tracking-wider">{label}</label>
            <input 
                type="text" 
                defaultValue={value} 
                className="w-full bg-background border border-input rounded-md px-3 py-2 text-sm focus:ring-1 focus:ring-blue-500 outline-none transition-all" 
            />
        </div>
    )
}

function Toggle({ checked, onChange }: any) {
    return (
        <div 
            onClick={onChange}
            className={`w-10 h-5 rounded-full relative cursor-pointer transition-colors ${checked ? 'bg-blue-600' : 'bg-secondary'}`}
        >
            <div className={`absolute top-1 w-3 h-3 bg-white rounded-full transition-all ${checked ? 'right-1' : 'left-1'}`}></div>
        </div>
    )
}
