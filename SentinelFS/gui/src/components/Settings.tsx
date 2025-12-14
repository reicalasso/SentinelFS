import { useState, useEffect } from 'react'
import { Shield, Smartphone, Globe, Lock, Database, Palette, Brain } from 'lucide-react'
import { useNotifications } from '../context/NotificationContext'
import {
  SettingsTab,
  GeneralSettings,
  NetworkSettings,
  SecuritySettings,
  AdvancedSettings,
  AppearanceSettings,
  FalconStoreSettings,
  Zer0Settings
} from './settings'

interface SettingsProps {
  config: any
}

export function Settings({ config }: SettingsProps) {
  const [activeTab, setActiveTab] = useState('general')
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
  
  // Load ignore patterns
  useEffect(() => {
    const loadPatterns = async () => {
      if (window.api) {
        await window.api.sendCommand('LIST_IGNORE')
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
  
  const handleUploadLimitChange = () => sendConfig('uploadLimit', uploadLimit)
  const handleDownloadLimitChange = () => sendConfig('downloadLimit', downloadLimit)
  
  const handleSessionCodeChange = () => {
    if (newSessionCode.length === 6) {
      sendConfig('sessionCode', newSessionCode)
      setNewSessionCode('')
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
  
  const handleExportConfig = async () => {
    if (window.api) {
      await window.api.sendCommand('EXPORT_CONFIG')
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
  
  const handleImportConfig = () => {
    const input = document.createElement('input')
    input.type = 'file'
    input.accept = '.json'
    input.onchange = async (e: any) => {
      const file = e.target.files[0]
      if (file && window.api) {
        const text = await file.text()
        await window.api.sendCommand(`IMPORT_CONFIG ${text}`)
        addNotification('success', 'Config Imported', 'Configuration loaded successfully')
      }
    }
    input.click()
  }
  
  const handleAddPattern = async () => {
    if (newPattern.trim() && window.api) {
      await window.api.sendCommand(`ADD_IGNORE ${newPattern.trim()}`)
      setIgnorePatterns(prev => [...prev, newPattern.trim()])
      setNewPattern('')
      addNotification('success', 'Pattern Added', `Now ignoring: ${newPattern}`)
    }
  }
  
  const handleRemovePattern = async (pattern: string) => {
    if (window.api) {
      await window.api.sendCommand(`REMOVE_IGNORE ${pattern}`)
      setIgnorePatterns(prev => prev.filter(p => p !== pattern))
      addNotification('info', 'Pattern Removed', `No longer ignoring: ${pattern}`)
    }
  }

  const handleExportSupportBundle = async () => {
    if (window.api) {
      addNotification('info', 'Generating...', 'Creating support bundle, please wait...')
      const res = await window.api.sendCommand('EXPORT_SUPPORT_BUNDLE')
      if (res.success) {
        const output = (res as any).output || ''
        const pathMatch = output.match(/BUNDLE_PATH:(.+)/)
        const bundlePath = pathMatch ? pathMatch[1].trim() : 'support folder'
        addNotification('success', 'Support Bundle Created', `Bundle saved to: ${bundlePath}`)
      } else {
        addNotification('error', 'Export Failed', res.error || 'Unknown error')
      }
    }
  }

  const handleRefreshStatus = () => window.api?.sendCommand('STATUS')

  const handleResetDefaults = () => {
    if (confirm('Are you sure you want to reset all settings to defaults?')) {
      sendConfig('uploadLimit', '0')
      sendConfig('downloadLimit', '0')
      addNotification('warning', 'Settings Reset', 'All settings have been reset to defaults')
    }
  }

  return (
    <div className="max-w-5xl mx-auto animate-in fade-in duration-500 slide-in-from-bottom-4">
      {/* Page Header */}
      <div className="settings-header">
        <div className="settings-header-icon">
          <Shield className="w-5 h-5 sm:w-6 sm:h-6 text-primary" />
        </div>
        <div>
          <h2 className="text-lg sm:text-xl font-bold">Settings</h2>
          <p className="text-xs sm:text-sm text-muted-foreground">Configure your SentinelFS instance</p>
        </div>
      </div>
      
      <div className="grid grid-cols-1 lg:grid-cols-4 gap-4 sm:gap-6">
        {/* Settings Navigation */}
        <nav className="settings-nav">
          <div className="flex lg:flex-col gap-1 sm:gap-2 min-w-max lg:min-w-0">
            <div className="hidden lg:block text-[10px] font-bold uppercase text-muted-foreground/60 tracking-widest px-3 mb-2">Navigation</div>
            <SettingsTab active={activeTab === 'general'} onClick={() => setActiveTab('general')} icon={<Smartphone className="w-4 h-4" />} label="General" />
            <SettingsTab active={activeTab === 'appearance'} onClick={() => setActiveTab('appearance')} icon={<Palette className="w-4 h-4" />} label="Appearance" />
            <SettingsTab active={activeTab === 'network'} onClick={() => setActiveTab('network')} icon={<Globe className="w-4 h-4" />} label="Network" />
            <SettingsTab active={activeTab === 'security'} onClick={() => setActiveTab('security')} icon={<Lock className="w-4 h-4" />} label="Security" />
            <SettingsTab active={activeTab === 'zer0'} onClick={() => setActiveTab('zer0')} icon={<Brain className="w-4 h-4" />} label="Zer0" />
            <SettingsTab active={activeTab === 'falconstore'} onClick={() => setActiveTab('falconstore')} icon={<Database className="w-4 h-4" />} label="FalconStore" />
            <SettingsTab active={activeTab === 'advanced'} onClick={() => setActiveTab('advanced')} icon={<Database className="w-4 h-4" />} label="Advanced" />
          </div>
        </nav>

        {/* Settings Content */}
        <div className="lg:col-span-3 space-y-4 sm:space-y-6">
          {activeTab === 'general' && (
            <GeneralSettings
              config={config}
              syncEnabled={syncEnabled}
              onSyncToggle={handleSyncToggle}
            />
          )}

          {activeTab === 'appearance' && <AppearanceSettings />}

          {activeTab === 'network' && (
            <NetworkSettings
              peerIp={peerIp}
              peerPort={peerPort}
              uploadLimit={uploadLimit}
              downloadLimit={downloadLimit}
              onPeerIpChange={setPeerIp}
              onPeerPortChange={setPeerPort}
              onUploadLimitChange={setUploadLimit}
              onDownloadLimitChange={setDownloadLimit}
              onAddPeer={handleAddPeer}
              onApplyUploadLimit={handleUploadLimitChange}
              onApplyDownloadLimit={handleDownloadLimitChange}
            />
          )}
          
          {activeTab === 'security' && (
            <SecuritySettings
              config={config}
              encryptionEnabled={encryptionEnabled}
              newSessionCode={newSessionCode}
              copied={copied}
              isGenerating={isGenerating}
              onEncryptionToggle={handleEncryptionToggle}
              onSessionCodeChange={setNewSessionCode}
              onGenerateCode={handleGenerateCode}
              onCopyCode={handleCopyCode}
              onSetSessionCode={handleSessionCodeChange}
            />
          )}

          {activeTab === 'zer0' && (
            <Zer0Settings onLog={(msg) => console.log(msg)} />
          )}

          {activeTab === 'falconstore' && (
            <FalconStoreSettings onLog={(msg) => console.log(msg)} />
          )}

          {activeTab === 'advanced' && (
            <AdvancedSettings
              config={config}
              ignorePatterns={ignorePatterns}
              newPattern={newPattern}
              onNewPatternChange={setNewPattern}
              onAddPattern={handleAddPattern}
              onRemovePattern={handleRemovePattern}
              onExportConfig={handleExportConfig}
              onImportConfig={handleImportConfig}
              onExportSupportBundle={handleExportSupportBundle}
              onRefreshStatus={handleRefreshStatus}
              onResetDefaults={handleResetDefaults}
            />
          )}
        </div>
      </div>
    </div>
  )
}
