import { Brain, Bug, AlertTriangle, FileWarning, Gauge } from 'lucide-react'
import { ThreatStatCard } from './ThreatStatCard'

interface ThreatStatusData {
  mlEnabled: boolean
  threatLevel?: 'SAFE' | 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL'
  threatScore?: number
  totalThreats?: number
  ransomwareAlerts?: number
  highEntropyFiles?: number
  avgFileEntropy?: number
}

interface ThreatAnalysisPanelProps {
  threatStatus: ThreatStatusData
  onOpenQuarantine?: () => void
}

export function ThreatAnalysisPanel({ threatStatus, onOpenQuarantine }: ThreatAnalysisPanelProps) {
  if (!threatStatus?.mlEnabled) {
    return null
  }

  const getThreatLevelClass = () => {
    switch (threatStatus.threatLevel) {
      case 'CRITICAL': return 'threat-level-critical'
      case 'HIGH': return 'threat-level-high'
      case 'MEDIUM': return 'threat-level-medium'
      case 'LOW': return 'threat-level-low'
      default: return 'threat-level-safe'
    }
  }

  const getThreatScoreClass = () => {
    const score = (threatStatus.threatScore || 0) * 100
    if (score >= 75) return 'threat-gauge-critical'
    if (score >= 50) return 'threat-gauge-high'
    if (score >= 25) return 'threat-gauge-medium'
    return 'threat-gauge-safe'
  }

  return (
    <div className="threat-panel cursor-pointer" onClick={onOpenQuarantine}>
      <div className="threat-panel-hover-overlay"></div>
      
      <div className="relative">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-3">
            <div className="threat-panel-icon">
              <Brain className="w-5 h-5 text-primary" />
            </div>
            <div>
              <h3 className="font-semibold">AI/ML Threat Analysis</h3>
              <p className="text-xs text-muted-foreground">Real-time security monitoring · Click to open</p>
            </div>
          </div>
          <div className={`threat-level-badge ${getThreatLevelClass()}`}>
            {threatStatus.threatLevel || 'Safe'}
          </div>
        </div>

        {/* Threat Score Gauge */}
        <div className="mb-4">
          <div className="flex items-center justify-between text-sm mb-2">
            <span className="text-muted-foreground">Threat Score</span>
            <span className="font-mono font-bold">{((threatStatus.threatScore || 0) * 100).toFixed(1)}%</span>
          </div>
          <div className="threat-gauge-track">
            <div 
              className={`threat-gauge-fill ${getThreatScoreClass()}`}
              style={{ width: `${Math.min(100, (threatStatus.threatScore || 0) * 100)}%` }}
            />
          </div>
        </div>

        {/* Threat Stats Grid */}
        <div className="grid grid-cols-2 sm:grid-cols-4 gap-3">
          <ThreatStatCard
            icon={<Bug className="w-4 h-4" />}
            label="Total Threats"
            value={threatStatus.totalThreats || 0}
            status={threatStatus.totalThreats && threatStatus.totalThreats > 0 ? 'warning' : 'success'}
          />
          <ThreatStatCard
            icon={<AlertTriangle className="w-4 h-4" />}
            label="Ransomware"
            value={threatStatus.ransomwareAlerts || 0}
            status={threatStatus.ransomwareAlerts && threatStatus.ransomwareAlerts > 0 ? 'error' : 'success'}
          />
          <ThreatStatCard
            icon={<FileWarning className="w-4 h-4" />}
            label="High Entropy"
            value={threatStatus.highEntropyFiles || 0}
            status={threatStatus.highEntropyFiles && threatStatus.highEntropyFiles > 0 ? 'warning' : 'success'}
          />
          <ThreatStatCard
            icon={<Gauge className="w-4 h-4" />}
            label="Avg Entropy"
            value={`${(threatStatus.avgFileEntropy || 0).toFixed(2)}`}
            status={threatStatus.avgFileEntropy && threatStatus.avgFileEntropy > 7.0 ? 'warning' : 'success'}
          />
        </div>
      </div>
    </div>
  )
}
