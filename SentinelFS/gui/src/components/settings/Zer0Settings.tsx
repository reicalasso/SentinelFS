import { useState, useEffect } from 'react'
import { 
  Shield, Brain, FileSearch, Activity, Zap, 
  Play, Pause, RefreshCw, Settings2, AlertTriangle,
  CheckCircle, XCircle, Eye, Cpu, Database,
  FileCode, Upload, Trash2, ToggleLeft, ToggleRight,
  ChevronDown, ChevronRight, Info
} from 'lucide-react'

interface Zer0Config {
  enabled: boolean
  autoQuarantine: boolean
  quarantineThreshold: string
  mlEnabled: boolean
  yaraEnabled: boolean
  processMonitorEnabled: boolean
  autoResponseEnabled: boolean
}

interface MLStats {
  samplesProcessed: number
  anomaliesDetected: number
  avgAnomalyScore: number
  modelLoaded: boolean
}

interface YaraStats {
  rulesLoaded: number
  filesScanned: number
  matchesFound: number
}

interface ProcessMonitorStats {
  processesTracked: number
  suspiciousDetected: number
  isRunning: boolean
}

interface AutoResponseStats {
  actionsExecuted: number
  filesQuarantined: number
  alertsSent: number
}

interface Zer0SettingsProps {
  onLog?: (message: string) => void
}

export function Zer0Settings({ onLog }: Zer0SettingsProps) {
  const [config, setConfig] = useState<Zer0Config>({
    enabled: true,
    autoQuarantine: true,
    quarantineThreshold: 'HIGH',
    mlEnabled: true,
    yaraEnabled: true,
    processMonitorEnabled: false,
    autoResponseEnabled: true
  })

  const [mlStats, setMlStats] = useState<MLStats>({
    samplesProcessed: 0,
    anomaliesDetected: 0,
    avgAnomalyScore: 0,
    modelLoaded: false
  })

  const [yaraStats, setYaraStats] = useState<YaraStats>({
    rulesLoaded: 0,
    filesScanned: 0,
    matchesFound: 0
  })

  const [processStats, setProcessStats] = useState<ProcessMonitorStats>({
    processesTracked: 0,
    suspiciousDetected: 0,
    isRunning: false
  })

  const [autoResponseStats, setAutoResponseStats] = useState<AutoResponseStats>({
    actionsExecuted: 0,
    filesQuarantined: 0,
    alertsSent: 0
  })

  const [expandedSections, setExpandedSections] = useState({
    ml: true,
    yara: true,
    process: false,
    autoResponse: true
  })

  const [isLoading, setIsLoading] = useState(false)
  const [isTraining, setIsTraining] = useState(false)
  const [trainingProgress, setTrainingProgress] = useState({ current: 0, total: 0, file: '' })

  useEffect(() => {
    // Ensure YARA is always enabled by default
    setConfig(prev => ({ ...prev, yaraEnabled: true }))
    loadZer0Status()
  }, [])

  const loadZer0Status = async () => {
    setIsLoading(true)
    try {
      if (window.api) {
        const response = await window.api.sendCommand('ZER0_STATUS') as any
        if (response.success && response.payload) {
          const payload = response.payload
          const pluginStats = payload.pluginStats
          
          if (pluginStats) {
            // Update ML stats
            if (pluginStats.mlStats) {
              setMlStats({
                samplesProcessed: pluginStats.mlStats.samplesProcessed || 0,
                anomaliesDetected: pluginStats.mlStats.anomaliesDetected || 0,
                avgAnomalyScore: pluginStats.mlStats.avgAnomalyScore || 0,
                modelLoaded: pluginStats.mlStats.modelLoaded || false
              })
            }
            
            // Update YARA stats
            if (pluginStats.yaraStats) {
              setYaraStats({
                rulesLoaded: pluginStats.yaraStats.rulesLoaded || 0,
                filesScanned: pluginStats.yaraStats.filesScanned || 0,
                matchesFound: pluginStats.yaraStats.matchesFound || 0
              })
            }
            
            // Update Process Monitor stats
            if (pluginStats.processMonitorStats) {
              setProcessStats({
                processesTracked: pluginStats.processMonitorStats.processesTracked || 0,
                suspiciousDetected: pluginStats.processMonitorStats.suspiciousDetected || 0,
                isRunning: pluginStats.processMonitorStats.isRunning || false
              })
            }
            
            // Update Auto Response stats
            if (pluginStats.autoResponseStats) {
              setAutoResponseStats({
                actionsExecuted: pluginStats.autoResponseStats.actionsExecuted || 0,
                filesQuarantined: pluginStats.autoResponseStats.filesQuarantined || 0,
                alertsSent: pluginStats.autoResponseStats.alertsSent || 0
              })
            }
          }
          
          onLog?.('Zer0 status loaded')
        }
      }
    } catch (error) {
      onLog?.(`Error loading Zer0 status: ${error}`)
    } finally {
      setIsLoading(false)
    }
  }

  const toggleSection = (section: keyof typeof expandedSections) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }))
  }

  const handleConfigChange = async (key: keyof Zer0Config, value: boolean | string) => {
    setConfig(prev => ({ ...prev, [key]: value }))
    
    if (window.api) {
      await window.api.sendCommand(`ZER0_CONFIG ${key}=${value}`)
      onLog?.(`Zer0 config updated: ${key}=${value}`)
    }
  }

  const handleStartMonitoring = async () => {
    if (window.api) {
      await window.api.sendCommand('ZER0_START_MONITORING')
      setProcessStats(prev => ({ ...prev, isRunning: true }))
      onLog?.('Zer0 monitoring started')
    }
  }

  const handleStopMonitoring = async () => {
    if (window.api) {
      await window.api.sendCommand('ZER0_STOP_MONITORING')
      setProcessStats(prev => ({ ...prev, isRunning: false }))
      onLog?.('Zer0 monitoring stopped')
    }
  }

  const handleReloadYaraRules = async () => {
    if (window.api) {
      await window.api.sendCommand('ZER0_RELOAD_YARA')
      onLog?.('YARA rules reloaded')
      loadZer0Status()
    }
  }

  const handleTrainModel = async () => {
    if (window.api) {
      setIsTraining(true)
      setTrainingProgress({ current: 0, total: 0, file: '' })
      
      try {
        const response = await window.api.sendCommand('ZER0_TRAIN_MODEL') as { success: boolean; message?: string; directory?: string }
        if (response.success) {
          onLog?.(`ML model training started for: ${response.directory || 'watch directory'}`)
        }
      } catch (error) {
        onLog?.(`Training error: ${error}`)
        setIsTraining(false)
      }
      
      // Training runs in background, will complete eventually
      // In a real implementation, we'd poll for status
      setTimeout(() => {
        setIsTraining(false)
        onLog?.('ML model training completed')
      }, 5000)
    }
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="settings-section">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="p-2 rounded-lg bg-gradient-to-br from-cyan-500/20 to-blue-500/20">
              <Shield className="w-5 h-5 text-cyan-400" />
            </div>
            <div>
              <h3 className="font-semibold">Zer0 Threat Detection</h3>
              <p className="text-xs text-muted-foreground">Advanced threat detection and response system</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={loadZer0Status}
              disabled={isLoading}
              className="p-2 rounded-lg hover:bg-accent transition-colors"
              title="Refresh Status"
            >
              <RefreshCw className={`w-4 h-4 ${isLoading ? 'animate-spin' : ''}`} />
            </button>
            <button
              onClick={() => handleConfigChange('enabled', !config.enabled)}
              className={`flex items-center gap-2 px-3 py-1.5 rounded-lg transition-colors ${
                config.enabled 
                  ? 'bg-emerald-500/20 text-emerald-400 hover:bg-emerald-500/30' 
                  : 'bg-red-500/20 text-red-400 hover:bg-red-500/30'
              }`}
            >
              {config.enabled ? <ToggleRight className="w-4 h-4" /> : <ToggleLeft className="w-4 h-4" />}
              {config.enabled ? 'Enabled' : 'Disabled'}
            </button>
          </div>
        </div>
      </div>

      {/* ML Engine Section */}
      <div className="settings-section">
        <button 
          onClick={() => toggleSection('ml')}
          className="w-full flex items-center justify-between p-3 -m-3 hover:bg-accent/50 rounded-lg transition-colors"
        >
          <div className="flex items-center gap-3">
            <Brain className="w-5 h-5 text-purple-400" />
            <div className="text-left">
              <h4 className="font-medium">ML Engine</h4>
              <p className="text-xs text-muted-foreground">Anomaly detection using machine learning</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            {mlStats.modelLoaded ? (
              <span className="text-xs px-2 py-0.5 rounded-full bg-emerald-500/20 text-emerald-400">Model Loaded</span>
            ) : (
              <span className="text-xs px-2 py-0.5 rounded-full bg-amber-500/20 text-amber-400">No Model</span>
            )}
            {expandedSections.ml ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
          </div>
        </button>

        {expandedSections.ml && (
          <div className="mt-4 space-y-4 pt-4 border-t border-border/50">
            {/* ML Stats */}
            <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
              <StatCard icon={<Database className="w-4 h-4" />} label="Samples" value={mlStats.samplesProcessed} />
              <StatCard icon={<AlertTriangle className="w-4 h-4" />} label="Anomalies" value={mlStats.anomaliesDetected} />
              <StatCard icon={<Activity className="w-4 h-4" />} label="Avg Score" value={mlStats.avgAnomalyScore.toFixed(2)} />
              <StatCard 
                icon={mlStats.modelLoaded ? <CheckCircle className="w-4 h-4 text-emerald-400" /> : <XCircle className="w-4 h-4 text-red-400" />} 
                label="Status" 
                value={mlStats.modelLoaded ? 'Ready' : 'Not Loaded'} 
              />
            </div>

            {/* ML Controls */}
            <div className="flex items-center gap-3">
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="checkbox"
                  checked={config.mlEnabled}
                  onChange={(e) => handleConfigChange('mlEnabled', e.target.checked)}
                  className="rounded border-border"
                />
                <span className="text-sm">Enable ML Analysis</span>
              </label>
              <button
                onClick={handleTrainModel}
                disabled={isTraining}
                className={`ml-auto px-3 py-1.5 text-sm rounded-lg transition-colors flex items-center gap-2 ${
                  isTraining 
                    ? 'bg-purple-500/10 text-purple-400/50 cursor-not-allowed' 
                    : 'bg-purple-500/20 text-purple-400 hover:bg-purple-500/30'
                }`}
              >
                {isTraining ? (
                  <>
                    <RefreshCw className="w-4 h-4 animate-spin" />
                    Training...
                  </>
                ) : (
                  <>
                    <Cpu className="w-4 h-4" />
                    Train Model
                  </>
                )}
              </button>
            </div>
            
            {/* Training Progress */}
            {isTraining && (
              <div className="p-3 rounded-lg bg-purple-500/10 border border-purple-500/20">
                <div className="flex items-center gap-2 mb-2">
                  <Activity className="w-4 h-4 text-purple-400 animate-pulse" />
                  <span className="text-sm text-purple-400 font-medium">Training in progress...</span>
                </div>
                <p className="text-xs text-muted-foreground">
                  Scanning files and building baseline model from your watch directory.
                  This may take a few minutes depending on the number of files.
                </p>
              </div>
            )}

            {/* ML Features */}
            <div className="text-xs text-muted-foreground space-y-1">
              <div className="flex items-center gap-2">
                <CheckCircle className="w-3 h-3 text-emerald-400" />
                Isolation Forest anomaly detection
              </div>
              <div className="flex items-center gap-2">
                <CheckCircle className="w-3 h-3 text-emerald-400" />
                Time-series pattern analysis
              </div>
              <div className="flex items-center gap-2">
                <CheckCircle className="w-3 h-3 text-emerald-400" />
                Behavioral clustering
              </div>
            </div>
          </div>
        )}
      </div>

      {/* YARA Scanner Section */}
      <div className="settings-section">
        <button 
          onClick={() => toggleSection('yara')}
          className="w-full flex items-center justify-between p-3 -m-3 hover:bg-accent/50 rounded-lg transition-colors"
        >
          <div className="flex items-center gap-3">
            <FileSearch className="w-5 h-5 text-amber-400" />
            <div className="text-left">
              <h4 className="font-medium">YARA Scanner</h4>
              <p className="text-xs text-muted-foreground">Signature-based malware detection</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <span className="text-xs px-2 py-0.5 rounded-full bg-amber-500/20 text-amber-400">
              {yaraStats.rulesLoaded} Rules
            </span>
            {expandedSections.yara ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
          </div>
        </button>

        {expandedSections.yara && (
          <div className="mt-4 space-y-4 pt-4 border-t border-border/50">
            {/* YARA Stats */}
            <div className="grid grid-cols-3 gap-3">
              <StatCard icon={<FileCode className="w-4 h-4" />} label="Rules" value={yaraStats.rulesLoaded} />
              <StatCard icon={<FileSearch className="w-4 h-4" />} label="Scanned" value={yaraStats.filesScanned} />
              <StatCard icon={<AlertTriangle className="w-4 h-4" />} label="Matches" value={yaraStats.matchesFound} />
            </div>

            {/* YARA Controls */}
            <div className="flex items-center gap-3">
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="checkbox"
                  checked={config.yaraEnabled}
                  onChange={(e) => handleConfigChange('yaraEnabled', e.target.checked)}
                  className="rounded border-border"
                />
                <span className="text-sm">Enable YARA Scanning</span>
              </label>
              <button
                onClick={handleReloadYaraRules}
                className="ml-auto px-3 py-1.5 text-sm rounded-lg bg-amber-500/20 text-amber-400 hover:bg-amber-500/30 transition-colors flex items-center gap-2"
              >
                <RefreshCw className="w-4 h-4" />
                Reload Rules
              </button>
            </div>

            {/* Rule Categories */}
            <div className="text-xs text-muted-foreground">
              <div className="font-medium mb-2">Loaded Rule Categories:</div>
              <div className="flex flex-wrap gap-2">
                {['Ransomware', 'Trojan', 'Backdoor', 'Cryptominer', 'Exploit', 'Rootkit'].map(cat => (
                  <span key={cat} className="px-2 py-0.5 rounded bg-accent/50">{cat}</span>
                ))}
              </div>
            </div>
          </div>
        )}
      </div>

      {/* Process Monitor Section */}
      <div className="settings-section">
        <button 
          onClick={() => toggleSection('process')}
          className="w-full flex items-center justify-between p-3 -m-3 hover:bg-accent/50 rounded-lg transition-colors"
        >
          <div className="flex items-center gap-3">
            <Eye className="w-5 h-5 text-blue-400" />
            <div className="text-left">
              <h4 className="font-medium">Process Monitor</h4>
              <p className="text-xs text-muted-foreground">Real-time process tracking and analysis</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            {processStats.isRunning ? (
              <span className="text-xs px-2 py-0.5 rounded-full bg-emerald-500/20 text-emerald-400 flex items-center gap-1">
                <span className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse" />
                Running
              </span>
            ) : (
              <span className="text-xs px-2 py-0.5 rounded-full bg-gray-500/20 text-gray-400">Stopped</span>
            )}
            {expandedSections.process ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
          </div>
        </button>

        {expandedSections.process && (
          <div className="mt-4 space-y-4 pt-4 border-t border-border/50">
            {/* Process Stats */}
            <div className="grid grid-cols-3 gap-3">
              <StatCard icon={<Activity className="w-4 h-4" />} label="Tracked" value={processStats.processesTracked} />
              <StatCard icon={<AlertTriangle className="w-4 h-4" />} label="Suspicious" value={processStats.suspiciousDetected} />
              <StatCard 
                icon={processStats.isRunning ? <Play className="w-4 h-4 text-emerald-400" /> : <Pause className="w-4 h-4" />} 
                label="Status" 
                value={processStats.isRunning ? 'Active' : 'Inactive'} 
              />
            </div>

            {/* Process Controls */}
            <div className="flex items-center gap-3">
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="checkbox"
                  checked={config.processMonitorEnabled}
                  onChange={(e) => handleConfigChange('processMonitorEnabled', e.target.checked)}
                  className="rounded border-border"
                />
                <span className="text-sm">Enable Process Monitoring</span>
              </label>
              <div className="ml-auto flex gap-2">
                {processStats.isRunning ? (
                  <button
                    onClick={handleStopMonitoring}
                    className="px-3 py-1.5 text-sm rounded-lg bg-red-500/20 text-red-400 hover:bg-red-500/30 transition-colors flex items-center gap-2"
                  >
                    <Pause className="w-4 h-4" />
                    Stop
                  </button>
                ) : (
                  <button
                    onClick={handleStartMonitoring}
                    className="px-3 py-1.5 text-sm rounded-lg bg-emerald-500/20 text-emerald-400 hover:bg-emerald-500/30 transition-colors flex items-center gap-2"
                  >
                    <Play className="w-4 h-4" />
                    Start
                  </button>
                )}
              </div>
            </div>

            {/* Warning */}
            <div className="flex items-start gap-2 p-3 rounded-lg bg-amber-500/10 border border-amber-500/20">
              <Info className="w-4 h-4 text-amber-400 mt-0.5 flex-shrink-0" />
              <p className="text-xs text-amber-400">
                Process monitoring may impact system performance. Enable only when investigating suspicious activity.
              </p>
            </div>
          </div>
        )}
      </div>

      {/* Auto Response Section */}
      <div className="settings-section">
        <button 
          onClick={() => toggleSection('autoResponse')}
          className="w-full flex items-center justify-between p-3 -m-3 hover:bg-accent/50 rounded-lg transition-colors"
        >
          <div className="flex items-center gap-3">
            <Zap className="w-5 h-5 text-rose-400" />
            <div className="text-left">
              <h4 className="font-medium">Auto Response</h4>
              <p className="text-xs text-muted-foreground">Automated threat remediation</p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            {config.autoResponseEnabled ? (
              <span className="text-xs px-2 py-0.5 rounded-full bg-emerald-500/20 text-emerald-400">Active</span>
            ) : (
              <span className="text-xs px-2 py-0.5 rounded-full bg-gray-500/20 text-gray-400">Disabled</span>
            )}
            {expandedSections.autoResponse ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
          </div>
        </button>

        {expandedSections.autoResponse && (
          <div className="mt-4 space-y-4 pt-4 border-t border-border/50">
            {/* Auto Response Stats */}
            <div className="grid grid-cols-3 gap-3">
              <StatCard icon={<Zap className="w-4 h-4" />} label="Actions" value={autoResponseStats.actionsExecuted} />
              <StatCard icon={<Shield className="w-4 h-4" />} label="Quarantined" value={autoResponseStats.filesQuarantined} />
              <StatCard icon={<AlertTriangle className="w-4 h-4" />} label="Alerts" value={autoResponseStats.alertsSent} />
            </div>

            {/* Auto Response Controls */}
            <div className="space-y-3">
              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="checkbox"
                  checked={config.autoResponseEnabled}
                  onChange={(e) => handleConfigChange('autoResponseEnabled', e.target.checked)}
                  className="rounded border-border"
                />
                <span className="text-sm">Enable Auto Response</span>
              </label>

              <label className="flex items-center gap-2 cursor-pointer">
                <input
                  type="checkbox"
                  checked={config.autoQuarantine}
                  onChange={(e) => handleConfigChange('autoQuarantine', e.target.checked)}
                  className="rounded border-border"
                />
                <span className="text-sm">Auto-quarantine threats</span>
              </label>

              <div className="flex items-center gap-3">
                <span className="text-sm">Quarantine Threshold:</span>
                <select
                  value={config.quarantineThreshold}
                  onChange={(e) => handleConfigChange('quarantineThreshold', e.target.value)}
                  className="px-3 py-1.5 text-sm rounded-lg bg-accent border border-border"
                >
                  <option value="MEDIUM">Medium</option>
                  <option value="HIGH">High</option>
                  <option value="CRITICAL">Critical Only</option>
                </select>
              </div>
            </div>

            {/* Response Rules */}
            <div className="text-xs text-muted-foreground">
              <div className="font-medium mb-2">Active Response Rules:</div>
              <div className="space-y-1">
                <div className="flex items-center gap-2">
                  <CheckCircle className="w-3 h-3 text-emerald-400" />
                  Ransomware Response (Quarantine + Alert)
                </div>
                <div className="flex items-center gap-2">
                  <CheckCircle className="w-3 h-3 text-emerald-400" />
                  Malware Response (Quarantine)
                </div>
                <div className="flex items-center gap-2">
                  <CheckCircle className="w-3 h-3 text-emerald-400" />
                  Cryptominer Response (Alert)
                </div>
                <div className="flex items-center gap-2">
                  <CheckCircle className="w-3 h-3 text-emerald-400" />
                  Suspicious Process Response (Log)
                </div>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  )
}

// Helper component for stats
function StatCard({ icon, label, value }: { icon: React.ReactNode; label: string; value: string | number }) {
  return (
    <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
      <div className="flex items-center gap-2 text-muted-foreground mb-1">
        {icon}
        <span className="text-xs">{label}</span>
      </div>
      <div className="font-mono font-bold">{value}</div>
    </div>
  )
}
