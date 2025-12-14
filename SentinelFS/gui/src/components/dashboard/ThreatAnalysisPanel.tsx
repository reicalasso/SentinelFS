import { Shield, Bug, AlertTriangle, FileWarning, Eye, Fingerprint, Activity, Brain, FileSearch, Zap } from 'lucide-react'
import { ThreatStatCard } from './ThreatStatCard'

interface Zer0StatusData {
  enabled: boolean
  threatLevel?: 'NONE' | 'INFO' | 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL'
  // Stats
  filesAnalyzed?: number
  threatsDetected?: number
  filesQuarantined?: number
  // Threat breakdown
  hiddenExecutables?: number
  extensionMismatches?: number
  ransomwarePatterns?: number
  behavioralAnomalies?: number
  // Capabilities
  magicByteValidation?: boolean
  behavioralAnalysis?: boolean
  fileTypeAwareness?: boolean
  // New capabilities
  mlEnabled?: boolean
  yaraEnabled?: boolean
  autoResponseEnabled?: boolean
  // ML stats
  mlAnomaliesDetected?: number
  yaraMatchesFound?: number
}

interface ThreatAnalysisPanelProps {
  threatStatus: Zer0StatusData
  onOpenQuarantine?: () => void
}

export function ThreatAnalysisPanel({ threatStatus, onOpenQuarantine }: ThreatAnalysisPanelProps) {
  if (!threatStatus?.enabled) {
    return null
  }

  const getThreatLevelClass = () => {
    switch (threatStatus.threatLevel) {
      case 'CRITICAL': return 'threat-level-critical'
      case 'HIGH': return 'threat-level-high'
      case 'MEDIUM': return 'threat-level-medium'
      case 'LOW': return 'threat-level-low'
      case 'INFO': return 'threat-level-info'
      default: return 'threat-level-safe'
    }
  }

  const getThreatLevelLabel = () => {
    switch (threatStatus.threatLevel) {
      case 'CRITICAL': return 'Critical'
      case 'HIGH': return 'High'
      case 'MEDIUM': return 'Medium'
      case 'LOW': return 'Low'
      case 'INFO': return 'Info'
      case 'NONE': return 'Safe'
      default: return 'Safe'
    }
  }

  const totalThreats = threatStatus.threatsDetected || 0

  return (
    <div className="threat-panel cursor-pointer" onClick={onOpenQuarantine}>
      <div className="threat-panel-hover-overlay"></div>
      
      <div className="relative">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-3">
            <div className="threat-panel-icon bg-gradient-to-br from-cyan-500/20 to-blue-500/20">
              <Shield className="w-5 h-5 text-cyan-400" />
            </div>
            <div>
              <h3 className="font-semibold flex items-center gap-2">
                Zer0 Threat Detection
                <span className="text-[10px] px-1.5 py-0.5 rounded bg-cyan-500/20 text-cyan-400 font-medium">
                  ADVANCED
                </span>
              </h3>
              <p className="text-xs text-muted-foreground">Magic bytes · Behavioral · File-aware · Click to open</p>
            </div>
          </div>
          <div className={`threat-level-badge ${getThreatLevelClass()}`}>
            {getThreatLevelLabel()}
          </div>
        </div>

        {/* Capabilities */}
        <div className="flex flex-wrap gap-2 mb-4">
          {threatStatus.magicByteValidation && (
            <span className="flex items-center gap-1 px-2 py-1 rounded-full text-[10px] bg-emerald-500/10 text-emerald-400 border border-emerald-500/20">
              <Fingerprint className="w-3 h-3" />
              Magic Bytes
            </span>
          )}
          {threatStatus.behavioralAnalysis && (
            <span className="flex items-center gap-1 px-2 py-1 rounded-full text-[10px] bg-purple-500/10 text-purple-400 border border-purple-500/20">
              <Activity className="w-3 h-3" />
              Behavioral
            </span>
          )}
          {threatStatus.fileTypeAwareness && (
            <span className="flex items-center gap-1 px-2 py-1 rounded-full text-[10px] bg-blue-500/10 text-blue-400 border border-blue-500/20">
              <Eye className="w-3 h-3" />
              Type-Aware
            </span>
          )}
          {threatStatus.mlEnabled && (
            <span className="flex items-center gap-1 px-2 py-1 rounded-full text-[10px] bg-pink-500/10 text-pink-400 border border-pink-500/20">
              <Brain className="w-3 h-3" />
              ML Engine
            </span>
          )}
          {threatStatus.yaraEnabled && (
            <span className="flex items-center gap-1 px-2 py-1 rounded-full text-[10px] bg-amber-500/10 text-amber-400 border border-amber-500/20">
              <FileSearch className="w-3 h-3" />
              YARA
            </span>
          )}
          {threatStatus.autoResponseEnabled && (
            <span className="flex items-center gap-1 px-2 py-1 rounded-full text-[10px] bg-rose-500/10 text-rose-400 border border-rose-500/20">
              <Zap className="w-3 h-3" />
              Auto-Response
            </span>
          )}
        </div>

        {/* Files Analyzed Progress */}
        <div className="mb-4">
          <div className="flex items-center justify-between text-sm mb-2">
            <span className="text-muted-foreground">Files Analyzed</span>
            <span className="font-mono font-bold">{threatStatus.filesAnalyzed?.toLocaleString() || 0}</span>
          </div>
          <div className="threat-gauge-track">
            <div 
              className="threat-gauge-fill bg-gradient-to-r from-cyan-500 to-blue-500"
              style={{ width: totalThreats > 0 ? '100%' : '0%' }}
            />
          </div>
          {totalThreats > 0 && (
            <div className="text-xs text-amber-400 mt-1">
              ⚠️ {totalThreats} threat{totalThreats > 1 ? 's' : ''} detected
            </div>
          )}
        </div>

        {/* Threat Stats Grid */}
        <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
          <ThreatStatCard
            icon={<Bug className="w-4 h-4" />}
            label="Quarantined"
            value={threatStatus.filesQuarantined || 0}
            status={threatStatus.filesQuarantined && threatStatus.filesQuarantined > 0 ? 'warning' : 'success'}
          />
          <ThreatStatCard
            icon={<AlertTriangle className="w-4 h-4" />}
            label="Ransomware"
            value={threatStatus.ransomwarePatterns || 0}
            status={threatStatus.ransomwarePatterns && threatStatus.ransomwarePatterns > 0 ? 'error' : 'success'}
          />
          <ThreatStatCard
            icon={<FileWarning className="w-4 h-4" />}
            label="Hidden Exec"
            value={threatStatus.hiddenExecutables || 0}
            status={threatStatus.hiddenExecutables && threatStatus.hiddenExecutables > 0 ? 'error' : 'success'}
          />
          <ThreatStatCard
            icon={<Activity className="w-4 h-4" />}
            label="Anomalies"
            value={threatStatus.behavioralAnomalies || 0}
            status={threatStatus.behavioralAnomalies && threatStatus.behavioralAnomalies > 0 ? 'warning' : 'success'}
          />
        </div>
      </div>
    </div>
  )
}
