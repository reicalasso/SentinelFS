import { Wifi, Zap, Network, HardDrive, Database, Brain, Shield } from 'lucide-react'
import { useState, useEffect } from 'react'
import { 
  StatCard, 
  HealthCard, 
  HeroSection, 
  NetworkTrafficChart, 
  ActivityFeed, 
  ThreatAnalysisPanel,
  DegradedPeersWarning 
} from './dashboard'

// Types - using any for flexibility with different data sources
interface MetricsData {
  totalUploaded?: number
  totalDownloaded?: number
  bytesUploaded?: number
  bytesDownloaded?: number
  [key: string]: any
}

interface SyncStatusData {
  syncStatus: string
  pendingFiles?: number
  health?: {
    healthy: boolean
    statusMessage: string
    activeWatcherCount?: number
    diskUsagePercent: number
    diskFreeBytes: number
    dbConnected: boolean
    dbSizeBytes: number
  }
  anomaly?: unknown
  peerHealth?: Array<{ degraded: boolean }>
}

interface ThreatStatusData {
  // Legacy format
  mlEnabled?: boolean
  threatLevel?: string
  threatScore?: number
  totalThreats?: number
  ransomwareAlerts?: number
  highEntropyFiles?: number
  avgFileEntropy?: number
  // Zer0 format
  enabled?: boolean
  filesAnalyzed?: number
  threatsDetected?: number
  filesQuarantined?: number
  hiddenExecutables?: number
  extensionMismatches?: number
  ransomwarePatterns?: number
  behavioralAnomalies?: number
  magicByteValidation?: boolean
  behavioralAnalysis?: boolean
  fileTypeAwareness?: boolean
}

interface ActivityData {
  type: string
  file?: string
  path?: string
  time?: string
  timestamp?: number
  details?: string
}

interface TrafficHistoryItem {
  time: string
  upload: number
  download: number
  uploadBytes: number
  downloadBytes: number
}

interface DashboardProps {
  metrics: MetricsData | null
  syncStatus: SyncStatusData | null
  peersCount: number
  activity: ActivityData[]
  threatStatus: ThreatStatusData | null
  onOpenQuarantine?: () => void
}

// Helper function
function formatBytes(bytes: number): string {
  if (!bytes || bytes === 0) return '0 B'
  if (bytes < 1024) return bytes + ' B'
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
  if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + ' MB'
  return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
}

export function Dashboard({ metrics, syncStatus, peersCount, activity, threatStatus, onOpenQuarantine }: DashboardProps) {
  const [trafficHistory, setTrafficHistory] = useState<TrafficHistoryItem[]>([])
  const [lastMetrics, setLastMetrics] = useState<MetricsData | null>(null)
  const [peakUpload, setPeakUpload] = useState(0)
  const [peakDownload, setPeakDownload] = useState(0)
  const [currentUploadRate, setCurrentUploadRate] = useState(0)
  const [currentDownloadRate, setCurrentDownloadRate] = useState(0)
  
  // Format metrics from daemon
  const totalUploaded = metrics?.totalUploaded || 0
  const totalDownloaded = metrics?.totalDownloaded || 0
  const recentActivity = activity || []
  
  // Health data from syncStatus
  const health = syncStatus?.health
  const peerHealth = syncStatus?.peerHealth || []
  const degradedPeers = peerHealth.filter((p) => p.degraded).length
  
  // Track network traffic over time
  useEffect(() => {
    if (!metrics) return
    
    const now = new Date()
    const timeStr = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`
    
    // Calculate delta (bytes/s then convert to appropriate unit)
    let uploadRate = 0
    let downloadRate = 0
    
    if (lastMetrics) {
      const timeDiff = 2 // seconds (polling interval)
      const currUp = metrics.totalUploaded || metrics.bytesUploaded || 0
      const lastUp = lastMetrics.totalUploaded || lastMetrics.bytesUploaded || 0
      const currDown = metrics.totalDownloaded || metrics.bytesDownloaded || 0
      const lastDown = lastMetrics.totalDownloaded || lastMetrics.bytesDownloaded || 0
      uploadRate = Math.max(0, (currUp - lastUp) / timeDiff)
      downloadRate = Math.max(0, (currDown - lastDown) / timeDiff)
      
      // Update current rates
      setCurrentUploadRate(uploadRate)
      setCurrentDownloadRate(downloadRate)
      
      // Track peaks
      if (uploadRate > peakUpload) setPeakUpload(uploadRate)
      if (downloadRate > peakDownload) setPeakDownload(downloadRate)
    }
    
    setLastMetrics(metrics)
    
    // Store as KB/s for chart
    setTrafficHistory(prev => {
      const newHistory = [...prev, { 
        time: timeStr, 
        upload: Math.round(uploadRate / 1024), 
        download: Math.round(downloadRate / 1024),
        uploadBytes: uploadRate,
        downloadBytes: downloadRate
      }]
      // Keep last 30 data points
      return newHistory.slice(-30)
    })
  }, [metrics, lastMetrics, peakUpload, peakDownload])

  return (
    <div className="space-y-4 sm:space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
      {/* Hero Section with Animated Stats */}
      <HeroSection 
        currentUpload={currentUploadRate}
        currentDownload={currentDownloadRate}
        peakUpload={peakUpload}
        peakDownload={peakDownload}
        peersCount={peersCount}
        formatBytes={formatBytes}
      />

      {/* Top Stats Row */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3 sm:gap-4">
        <StatCard 
          title="Network Status" 
          value={metrics ? "Online" : "Connecting"}
          sub="Direct P2P Mesh"
          icon={<Wifi className="w-5 h-5 icon-success" />}
          status={metrics ? "success" : "warning"}
        />
        <StatCard 
          title="Sync Status" 
          value={syncStatus?.syncStatus || "Idle"} 
          sub={syncStatus?.pendingFiles ? `${syncStatus.pendingFiles} files pending` : "Up to date"}
          icon={<Zap className="w-5 h-5 text-primary" />}
          status={syncStatus?.syncStatus === 'ENABLED' ? "success" : "warning"}
        />
        <StatCard 
          title="Active Peers" 
          value={`${peersCount || 0} Devices`} 
          sub="Mesh Swarm"
          icon={<Network className="w-5 h-5 text-primary" />}
        />
        <StatCard 
          title="Total Traffic" 
          value={formatBytes(totalDownloaded)} 
          sub={`↑ ${formatBytes(totalUploaded)}`}
          icon={<HardDrive className="w-5 h-5 icon-info" />}
        />
      </div>

      {/* Main Content Grid */}
      <div className="grid grid-cols-1 xl:grid-cols-3 gap-4 sm:gap-6">
        {/* Chart Section */}
        <NetworkTrafficChart 
          trafficHistory={trafficHistory}
          totalDownloaded={totalDownloaded}
          totalUploaded={totalUploaded}
          peakDownload={peakDownload}
          peakUpload={peakUpload}
          formatBytes={formatBytes}
        />

        {/* Activity Feed */}
        <ActivityFeed recentActivity={recentActivity as any} />
      </div>

      {/* Health Summary Row */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3 sm:gap-4">
        <HealthCard
          title="System Health"
          icon={<Shield className="w-5 h-5" />}
          status={health?.healthy ? 'success' : 'warning'}
          value={health?.statusMessage || 'Unknown'}
          sub={health ? `${health.activeWatcherCount || 0} active watcher${(health.activeWatcherCount || 0) !== 1 ? 's' : ''}` : 'Loading...'}
        />
        <HealthCard
          title="Disk Usage"
          icon={<HardDrive className="w-5 h-5" />}
          status={health?.diskUsagePercent && health.diskUsagePercent > 90 ? 'error' : health?.diskUsagePercent && health.diskUsagePercent > 75 ? 'warning' : 'success'}
          value={health ? `${health.diskUsagePercent.toFixed(1)}%` : '--'}
          sub={health ? `${(health.diskFreeBytes / (1024 * 1024 * 1024)).toFixed(1)} GB free` : 'Loading...'}
        />
        <HealthCard
          title="Database"
          icon={<Database className="w-5 h-5" />}
          status={health?.dbConnected ? 'success' : 'error'}
          value={health?.dbConnected ? 'Connected' : 'Disconnected'}
          sub={health ? `${(health.dbSizeBytes / 1024).toFixed(0)} KB` : 'Loading...'}
        />
        <HealthCard
          title="Zer0 Threat Detection"
          icon={<Shield className="w-5 h-5" />}
          status={threatStatus?.threatLevel === 'HIGH' || threatStatus?.threatLevel === 'CRITICAL' ? 'error' : threatStatus?.threatLevel === 'MEDIUM' ? 'warning' : 'success'}
          value={threatStatus?.enabled || threatStatus?.mlEnabled ? threatStatus.threatLevel || 'Safe' : 'Disabled'}
          sub={threatStatus?.enabled || threatStatus?.mlEnabled ? `${threatStatus.filesAnalyzed || 0} files analyzed` : 'Enable Zer0 plugin'}
        />
      </div>

      {/* Zer0 Threat Detection Panel */}
      {(threatStatus?.enabled || threatStatus?.mlEnabled) && (
        <ThreatAnalysisPanel 
          threatStatus={{
            enabled: threatStatus.enabled || threatStatus.mlEnabled || false,
            threatLevel: threatStatus.threatLevel as any,
            filesAnalyzed: threatStatus.filesAnalyzed || 0,
            threatsDetected: threatStatus.threatsDetected || threatStatus.totalThreats || 0,
            filesQuarantined: threatStatus.filesQuarantined || 0,
            hiddenExecutables: threatStatus.hiddenExecutables || 0,
            extensionMismatches: threatStatus.extensionMismatches || 0,
            ransomwarePatterns: threatStatus.ransomwarePatterns || threatStatus.ransomwareAlerts || 0,
            behavioralAnomalies: threatStatus.behavioralAnomalies || 0,
            magicByteValidation: threatStatus.magicByteValidation ?? true,
            behavioralAnalysis: threatStatus.behavioralAnalysis ?? true,
            fileTypeAwareness: threatStatus.fileTypeAwareness ?? true,
          }} 
          onOpenQuarantine={onOpenQuarantine} 
        />
      )}

      {/* Degraded Peers Warning */}
      <DegradedPeersWarning degradedPeers={degradedPeers} />
    </div>
  )
}
