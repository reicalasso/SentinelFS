import { useState, useEffect } from 'react'
import { Database, Globe, Lock, Shield, Smartphone, Save, RefreshCw, Copy, Check, Key, Moon, Sun, Download, Upload, Plus, X, FileX } from 'lucide-react'
import { useTheme } from '../context/ThemeContext'
import { useNotifications } from '../context/NotificationContext'

interface SettingsProps {
  config: any
}

export function Settings({ config }: SettingsProps) {
  const [activeTab, setActiveTab] = useState('general')
  const { theme, toggleTheme } = useTheme()
  const { addNotification } = useNotifications()
  
  // Local state for form inputs
  const [uploadLimit, setUploadLimit] = useState('0')
  const [downloadLimit, setDownloadLimit] = useState('0')
  const [newSessionCode, setNewSessionCode] = useState('')
  const [encryptionEnabled, setEncryptionEnabled] = useState(false)
  const [syncEnabled, setSyncEnabled] = useState(true)
  const [copied, setCopied] = useState(false)
  const [isGenerating, setIsGenerating] = useState(false)
  
  // Ignore patterns state
  const [ignorePatterns, setIgnorePatterns] = useState<string[]>([])
  const [newPattern, setNewPattern] = useState('')
  
  // Manual peer connection state
  const [peerIp, setPeerIp] = useState('')
  const [peerPort, setPeerPort] = useState('8080')
  
  // Format session code as ABC-123
  const formatCode = (code: string) => {
    if (!code || code.length !== 6) return code
    return `${code.slice(0, 3)}-${code.slice(3)}`
  }
  
  // Load ignore patterns
  useEffect(() => {
    const loadPatterns = async () => {
      if (window.api) {
        const res = await window.api.sendCommand('LIST_IGNORE')
        // Parse response - it will be handled by daemon-data event
      }
    }
    loadPatterns()
  }, [])
  
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
    if (newSessionCode.length === 6) {
      sendConfig('sessionCode', newSessionCode)
      setNewSessionCode('') // Clear after setting
    }
  }
  
  const handleGenerateCode = async () => {
    if (window.api) {
      setIsGenerating(true)
      await window.api.sendCommand('GENERATE_CODE')
      setTimeout(() => setIsGenerating(false), 1000)
    }
  }
  
  const handleCopyCode = () => {
    if (config?.sessionCode) {
      navigator.clipboard.writeText(config.sessionCode)
      setCopied(true)
      setTimeout(() => setCopied(false), 2000)
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
  
  const handleAddPeer = async () => {
    if (!peerIp || !peerPort) {
      addNotification('error', 'Validation Error', 'Please enter both IP address and port')
      return
    }
    
    if (window.api) {
      const res = await window.api.sendCommand(`ADD_PEER ${peerIp}:${peerPort}`)
      if (res.success) {
        addNotification('success', 'Peer Added', `Connecting to ${peerIp}:${peerPort}`)
        setPeerIp('')
        setPeerPort('8080')
      } else {
        addNotification('error', 'Connection Failed', res.error || 'Unknown error')
      }
    }
  }
  
  // Export config
  const handleExportConfig = async () => {
    if (window.api) {
      const res = await window.api.sendCommand('EXPORT_CONFIG')
      // Create download
      const blob = new Blob([JSON.stringify(config, null, 2)], { type: 'application/json' })
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = 'sentinelfs-config.json'
      a.click()
      URL.revokeObjectURL(url)
      addNotification('success', 'Config Exported', 'Configuration saved to sentinelfs-config.json')
    }
  }
  
  // Import config
  const handleImportConfig = () => {
    const input = document.createElement('input')
    input.type = 'file'
    input.accept = '.json'
    input.onchange = async (e: any) => {
      const file = e.target.files[0]
      if (file) {
        const text = await file.text()
        if (window.api) {
          await window.api.sendCommand(`IMPORT_CONFIG ${text}`)
          addNotification('success', 'Config Imported', 'Configuration loaded successfully')
        }
      }
    }
    input.click()
  }
  
  // Add ignore pattern
  const handleAddPattern = async () => {
    if (newPattern.trim() && window.api) {
      await window.api.sendCommand(`ADD_IGNORE ${newPattern.trim()}`)
      setIgnorePatterns(prev => [...prev, newPattern.trim()])
      setNewPattern('')
      addNotification('success', 'Pattern Added', `Now ignoring: ${newPattern}`)
    }
  }
  
  // Remove ignore pattern
  const handleRemovePattern = async (pattern: string) => {
    if (window.api) {
      await window.api.sendCommand(`REMOVE_IGNORE ${pattern}`)
      setIgnorePatterns(prev => prev.filter(p => p !== pattern))
      addNotification('info', 'Pattern Removed', `No longer ignoring: ${pattern}`)
    }
  }

  return (
    <div className="max-w-5xl mx-auto animate-in fade-in duration-500 slide-in-from-bottom-4">
      {/* Page Header */}
      <div className="flex items-center gap-4 mb-8">
        <div className="p-3 rounded-2xl bg-gradient-to-br from-primary/15 to-primary/5 border border-primary/20">
          <Shield className="w-6 h-6 text-primary" />
        </div>
        <div>
          <h2 className="text-xl font-bold">Settings</h2>
          <p className="text-sm text-muted-foreground">Configure your SentinelFS instance</p>
        </div>
      </div>
      
      <div className="grid grid-cols-1 md:grid-cols-4 gap-6">
        {/* Settings Navigation */}
        <nav className="space-y-2 bg-card/30 p-3 rounded-2xl border border-border/30 h-fit sticky top-4">
            <div className="text-[10px] font-bold uppercase text-muted-foreground/60 tracking-widest px-3 mb-3">Navigation</div>
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
                    <Section title="Appearance">
                        <div className="flex items-center justify-between">
                            <div className="flex items-center gap-3">
                                {theme === 'dark' ? <Moon className="w-5 h-5 text-blue-500" /> : <Sun className="w-5 h-5 text-yellow-500" />}
                                <div>
                                    <span className="text-sm font-medium block">Theme</span>
                                    <span className="text-xs text-muted-foreground">{theme === 'dark' ? 'Dark Mode' : 'Light Mode'}</span>
                                </div>
                            </div>
                            <Toggle checked={theme === 'dark'} onChange={toggleTheme} />
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
                            
                            {/* Display additional watched folders */}
                            {config?.watchedFolders && config.watchedFolders.length > 0 && (
                                <div className="pt-2 mt-2 border-t border-border/50">
                                    <span className="text-muted-foreground block mb-1">Additional Watch Folders:</span>
                                    <div className="space-y-1 pl-1">
                                        {config.watchedFolders.map((folder: string, i: number) => (
                                            <div key={i} className="flex justify-between items-center group">
                                                <span className="font-mono text-xs break-all">{folder}</span>
                                                <button 
                                                    onClick={async () => {
                                                        if (confirm(`Stop watching ${folder}?\n\nFiles will remain on disk, only monitoring will stop.`)) {
                                                            if (window.api) await window.api.sendCommand(`REMOVE_WATCH ${folder}`)
                                                        }
                                                    }}
                                                    className="opacity-0 group-hover:opacity-100 p-1 hover:bg-red-500/10 text-red-500 rounded transition-opacity"
                                                    title="Stop watching (files will NOT be deleted)"
                                                >
                                                    <X className="w-3 h-3" />
                                                </button>
                                            </div>
                                        ))}
                                    </div>
                                </div>
                            )}
                        </div>
                    </Section>
                </div>
            )}

            {activeTab === 'network' && (
                <div className="space-y-6">
                    <Section title="Manual Peer Connection">
                        <div className="bg-primary/5 border border-primary/10 rounded-xl p-4 mb-4">
                            <div className="flex items-start gap-3 mb-4">
                                <div className="p-2 rounded-lg bg-primary/10">
                                    <Globe className="w-4 h-4 text-primary" />
                                </div>
                                <div>
                                    <span className="text-sm font-semibold">Connect to a specific peer</span>
                                    <p className="text-xs text-muted-foreground mt-1">Enter the IP address and port of a peer on a different network</p>
                                </div>
                            </div>
                            <div className="grid grid-cols-[1fr_auto_auto] gap-2">
                                <input 
                                    type="text" 
                                    value={peerIp}
                                    onChange={(e) => setPeerIp(e.target.value)}
                                    placeholder="192.168.1.100"
                                    className="bg-background/50 border border-border/50 rounded-xl px-4 py-2.5 text-sm focus:ring-2 focus:ring-primary/20 focus:border-primary/30 outline-none transition-all" 
                                />
                                <input 
                                    type="number" 
                                    value={peerPort}
                                    onChange={(e) => setPeerPort(e.target.value)}
                                    placeholder="8080"
                                    className="w-24 bg-background/50 border border-border/50 rounded-xl px-4 py-2.5 text-sm focus:ring-2 focus:ring-primary/20 focus:border-primary/30 outline-none transition-all" 
                                />
                                <button 
                                    onClick={handleAddPeer}
                                    className="px-4 py-2.5 bg-primary hover:bg-primary/90 text-primary-foreground rounded-xl text-sm font-medium transition-all flex items-center gap-2 shadow-sm"
                                >
                                    <Plus className="w-4 h-4" />
                                    Connect
                                </button>
                            </div>
                        </div>
                    </Section>
                    
                    <Section title="Bandwidth Limits">
                        <div className="mb-5">
                            <label className="block text-xs font-medium text-muted-foreground mb-2 uppercase tracking-wider">Upload Limit (KB/s)</label>
                            <div className="flex gap-2">
                                <input 
                                    type="number" 
                                    value={uploadLimit}
                                    onChange={(e) => setUploadLimit(e.target.value)}
                                    placeholder="0 = Unlimited"
                                    className="flex-1 bg-background/50 border border-border/50 rounded-xl px-4 py-2.5 text-sm focus:ring-2 focus:ring-primary/20 focus:border-primary/30 outline-none transition-all" 
                                />
                                <button 
                                    onClick={handleUploadLimitChange}
                                    className="px-4 py-2.5 bg-primary hover:bg-primary/90 text-primary-foreground rounded-xl text-sm font-medium transition-all flex items-center gap-2 shadow-sm"
                                >
                                    <Save className="w-4 h-4" />
                                    Apply
                                </button>
                            </div>
                            <div className="text-xs text-muted-foreground mt-1.5">0 = Unlimited</div>
                        </div>
                        <div className="mb-4">
                            <label className="block text-xs font-medium text-muted-foreground mb-2 uppercase tracking-wider">Download Limit (KB/s)</label>
                            <div className="flex gap-2">
                                <input 
                                    type="number" 
                                    value={downloadLimit}
                                    onChange={(e) => setDownloadLimit(e.target.value)}
                                    placeholder="0 = Unlimited"
                                    className="flex-1 bg-background/50 border border-border/50 rounded-xl px-4 py-2.5 text-sm focus:ring-2 focus:ring-primary/20 focus:border-primary/30 outline-none transition-all" 
                                />
                                <button 
                                    onClick={handleDownloadLimitChange}
                                    className="px-4 py-2.5 bg-primary hover:bg-primary/90 text-primary-foreground rounded-xl text-sm font-medium transition-all flex items-center gap-2 shadow-sm"
                                >
                                    <Save className="w-4 h-4" />
                                    Apply
                                </button>
                            </div>
                            <div className="text-xs text-muted-foreground mt-1.5">0 = Unlimited</div>
                        </div>
                    </Section>
                </div>
            )}
            
            {activeTab === 'security' && (
                <div className="space-y-6">
                    <Section title="Session Code">
                        {/* Current Code Display */}
                        {config?.sessionCode && (
                            <div className="mb-6 p-5 bg-primary/5 border border-primary/15 rounded-xl">
                                <div className="flex items-center justify-between">
                                    <div className="flex items-center gap-4">
                                        <div className="p-3 rounded-xl bg-primary/10 border border-primary/20">
                                            <Key className="w-5 h-5 text-primary" />
                                        </div>
                                        <div>
                                            <div className="text-xs text-muted-foreground uppercase tracking-wider">Current Session Code</div>
                                            <div className="font-mono text-2xl font-bold tracking-widest text-primary mt-1">
                                                {formatCode(config.sessionCode)}
                                            </div>
                                        </div>
                                    </div>
                                    <button 
                                        onClick={handleCopyCode}
                                        className="p-2.5 rounded-xl bg-secondary/50 hover:bg-secondary transition-colors border border-border/50"
                                        title="Copy to clipboard"
                                    >
                                        {copied ? <Check className="w-5 h-5 text-emerald-500" /> : <Copy className="w-5 h-5 text-muted-foreground" />}
                                    </button>
                                </div>
                                <p className="text-xs text-muted-foreground mt-3">Share this code with peers to allow them to connect.</p>
                            </div>
                        )}
                        
                        {/* No Code Warning */}
                        {!config?.sessionCode && (
                            <div className="mb-6 p-4 bg-amber-500/10 border border-amber-500/20 rounded-xl">
                                <div className="flex items-center gap-2 text-amber-500 mb-2">
                                    <Shield className="w-4 h-4" />
                                    <span className="text-sm font-semibold">No Session Code Set</span>
                                </div>
                                <p className="text-xs text-muted-foreground">Any peer can connect to your device. Set a session code for secure pairing.</p>
                            </div>
                        )}
                        
                        {/* Generate or Set Code */}
                        <div className="space-y-4">
                            <div>
                                <label className="block text-xs font-medium text-muted-foreground mb-2 uppercase tracking-wider">Quick Generate</label>
                                <button 
                                    onClick={handleGenerateCode}
                                    disabled={isGenerating}
                                    className="w-full py-3 bg-primary hover:bg-primary/90 disabled:bg-primary/50 text-primary-foreground rounded-xl text-sm font-medium transition-all flex items-center justify-center gap-2 shadow-sm"
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
                                <label className="block text-xs font-medium text-muted-foreground mb-2 uppercase tracking-wider">Manual Entry</label>
                                <div className="flex gap-2">
                                    <input 
                                        type="text" 
                                        value={newSessionCode}
                                        onChange={(e) => setNewSessionCode(e.target.value.toUpperCase().replace(/[^A-Z0-9]/g, ''))}
                                        maxLength={6}
                                        placeholder="ABC123"
                                        className="flex-1 bg-background/50 border border-border/50 rounded-xl px-4 py-2.5 text-sm font-mono focus:ring-2 focus:ring-primary/20 focus:border-primary/30 outline-none transition-all tracking-widest text-center" 
                                    />
                                    <button 
                                        onClick={handleSessionCodeChange}
                                        disabled={newSessionCode.length !== 6}
                                        className="px-4 py-2.5 bg-secondary hover:bg-secondary/80 disabled:opacity-50 text-foreground rounded-xl text-sm font-medium transition-all flex items-center gap-2"
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
                            <Toggle checked={encryptionEnabled} onChange={handleEncryptionToggle} />
                        </div>
                        {encryptionEnabled ? (
                            <div className="bg-emerald-500/10 p-4 rounded-xl border border-emerald-500/20">
                                <div className="flex items-center gap-2 text-emerald-500 mb-2">
                                    <Shield className="w-4 h-4" />
                                    <span className="text-sm font-semibold">Encryption Active</span>
                                </div>
                                <p className="text-xs text-muted-foreground">All traffic is encrypted with AES-256-CBC + HMAC verification.</p>
                            </div>
                        ) : (
                            <div className="bg-secondary/30 p-4 rounded-xl border border-border/50">
                                <p className="text-xs text-muted-foreground">⚠ Traffic is not encrypted. Enable encryption for secure file transfers.</p>
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
                    <Section title="Ignore Patterns">
                        <p className="text-xs text-muted-foreground mb-4">Files and folders matching these patterns will not be synchronized.</p>
                        <div className="space-y-3">
                            <div className="flex gap-2">
                                <input 
                                    type="text"
                                    value={newPattern}
                                    onChange={(e) => setNewPattern(e.target.value)}
                                    placeholder="*.log, node_modules/, .git/"
                                    className="flex-1 bg-background/50 border border-border/50 rounded-xl px-4 py-2.5 text-sm font-mono focus:ring-2 focus:ring-primary/20 focus:border-primary/30 outline-none"
                                    onKeyDown={(e) => e.key === 'Enter' && handleAddPattern()}
                                />
                                <button 
                                    onClick={handleAddPattern}
                                    disabled={!newPattern.trim()}
                                    className="px-4 py-2.5 bg-primary hover:bg-primary/90 disabled:opacity-50 text-primary-foreground rounded-xl text-sm font-medium transition-all flex items-center gap-2 shadow-sm"
                                >
                                    <Plus className="w-4 h-4" /> Add
                                </button>
                            </div>
                            {ignorePatterns.length > 0 ? (
                                <div className="space-y-2">
                                    {ignorePatterns.map((pattern, i) => (
                                        <div key={i} className="flex items-center justify-between p-3 bg-secondary/30 rounded-xl border border-border/30">
                                            <div className="flex items-center gap-2">
                                                <FileX className="w-4 h-4 text-muted-foreground" />
                                                <span className="font-mono text-sm">{pattern}</span>
                                            </div>
                                            <button 
                                                onClick={() => handleRemovePattern(pattern)}
                                                className="p-1.5 hover:bg-destructive/10 rounded-lg text-muted-foreground hover:text-destructive transition-colors"
                                            >
                                                <X className="w-4 h-4" />
                                            </button>
                                        </div>
                                    ))}
                                </div>
                            ) : (
                                <p className="text-xs text-muted-foreground text-center py-6 bg-secondary/20 rounded-xl border border-dashed border-border/50">No ignore patterns defined</p>
                            )}
                        </div>
                    </Section>
                    <Section title="Export / Import">
                        <div className="grid grid-cols-2 gap-4">
                            <button 
                                onClick={handleExportConfig}
                                className="flex items-center justify-center gap-2 px-4 py-3.5 bg-secondary/50 hover:bg-secondary transition-all text-foreground rounded-xl text-sm font-medium border border-border/30 hover:border-border/50"
                            >
                                <Download className="w-4 h-4" /> Export Config
                            </button>
                            <button 
                                onClick={handleImportConfig}
                                className="flex items-center justify-center gap-2 px-4 py-3.5 bg-secondary/50 hover:bg-secondary transition-all text-foreground rounded-xl text-sm font-medium border border-border/30 hover:border-border/50"
                            >
                                <Upload className="w-4 h-4" /> Import Config
                            </button>
                        </div>
                        <p className="text-xs text-muted-foreground mt-3">Export settings to share with other devices or backup your configuration.</p>
                    </Section>
                    <Section title="Support Bundle">
                        <p className="text-xs text-muted-foreground mb-4">
                          Generate a support bundle containing config, logs, and system info for troubleshooting.
                        </p>
                        <button 
                            onClick={async () => {
                                if (window.api) {
                                    addNotification('info', 'Generating...', 'Creating support bundle, please wait...')
                                    const res = await window.api.sendCommand('EXPORT_SUPPORT_BUNDLE')
                                    if (res.success) {
                                        // Parse bundle path from response
                                        const output = (res as any).output || ''
                                        const pathMatch = output.match(/BUNDLE_PATH:(.+)/)
                                        const bundlePath = pathMatch ? pathMatch[1].trim() : 'support folder'
                                        addNotification('success', 'Support Bundle Created', `Bundle saved to: ${bundlePath}`)
                                    } else {
                                        addNotification('error', 'Export Failed', res.error || 'Unknown error')
                                    }
                                }
                            }}
                            className="w-full flex items-center justify-center gap-2 px-4 py-3.5 bg-primary/10 hover:bg-primary/20 text-primary border border-primary/20 hover:border-primary/30 rounded-xl text-sm font-medium transition-all"
                        >
                            <Download className="w-4 h-4" /> Export Support Bundle
                        </button>
                    </Section>
                    <Section title="ML Anomaly Detection">
                        <div className="bg-amber-500/10 p-4 rounded-xl border border-amber-500/20">
                            <div className="flex items-center gap-2 text-amber-500 mb-2">
                                <Database className="w-4 h-4" />
                                <span className="text-sm font-semibold">Experimental Feature</span>
                            </div>
                            <p className="text-xs text-muted-foreground">ML-based anomaly detection uses IsolationForest via ONNX Runtime to identify suspicious peer behavior. This feature is in beta.</p>
                        </div>
                    </Section>
                    <Section title="Danger Zone">
                        <div className="space-y-3">
                            <button 
                                onClick={() => window.api?.sendCommand('STATUS')}
                                className="w-full px-4 py-3 bg-secondary/50 hover:bg-secondary transition-all text-foreground rounded-xl text-sm font-medium border border-border/30 hover:border-border/50"
                            >
                                Refresh Daemon Status
                            </button>
                            <button 
                                className="w-full px-4 py-3 bg-destructive/10 hover:bg-destructive/20 text-destructive border border-destructive/20 hover:border-destructive/30 rounded-xl text-sm font-medium transition-all"
                                onClick={() => {
                                    if (confirm('Are you sure you want to reset all settings to defaults?')) {
                                        sendConfig('uploadLimit', '0')
                                        sendConfig('downloadLimit', '0')
                                        addNotification('warning', 'Settings Reset', 'All settings have been reset to defaults')
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
            className={`w-full flex items-center gap-3 px-4 py-2.5 rounded-xl text-sm font-medium transition-all duration-200 ${
                active 
                  ? 'bg-primary/10 text-primary border border-primary/20 shadow-sm' 
                  : 'text-muted-foreground hover:bg-secondary/60 hover:text-foreground'
            }`}
        >
            <span className={`transition-colors ${active ? 'text-primary' : ''}`}>{icon}</span>
            {label}
            {active && <div className="ml-auto w-1.5 h-1.5 rounded-full bg-primary"></div>}
        </button>
    )
}

function Section({ title, children }: any) {
    return (
        <div className="bg-card/50 backdrop-blur-sm border border-border/50 rounded-2xl p-6 shadow-sm hover:border-border/80 transition-colors">
            <h3 className="font-semibold mb-4 pb-3 border-b border-border/50 text-foreground flex items-center gap-2">
                <div className="w-1 h-4 bg-primary/60 rounded-full"></div>
                {title}
            </h3>
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
                className="w-full bg-background/50 border border-border/50 rounded-xl px-4 py-2.5 text-sm focus:ring-2 focus:ring-primary/20 focus:border-primary/30 outline-none transition-all" 
            />
        </div>
    )
}

function Toggle({ checked, onChange }: any) {
    return (
        <div 
            onClick={onChange}
            className={`w-11 h-6 rounded-full relative cursor-pointer transition-all duration-300 ${
                checked 
                  ? 'bg-primary shadow-sm shadow-primary/30' 
                  : 'bg-secondary/80 border border-border/50'
            }`}
        >
            <div className={`absolute top-1 w-4 h-4 bg-white rounded-full shadow-sm transition-all duration-300 ${checked ? 'right-1' : 'left-1'}`}></div>
        </div>
    )
}
