import { Wifi, Zap, Network, HardDrive, Brain, Shield, Database } from 'lucide-react'
import { useState, useEffect, memo, useMemo, useCallback } from 'react'
import { useFileActivity } from '../hooks/useFileActivity'
import { 
  StatCard, 
  HealthCard, 
  HeroSection, 
  NetworkTrafficChart, 
  FileActivityChart,
  ActivityFeed,
  type ActivityData,
  ThreatAnalysisPanel,
  DegradedPeersWarning 
} from './dashboard'

// Constants
const POLLING_INTERVAL_SECONDS = 2
const MAX_TRAFFIC_HISTORY_POINTS = 30
const BYTES_TO_KB = 1024
const BYTES_TO_MB = 1024 * 1024
const BYTES_TO_GB = 1024 * 1024 * 1024
const DISK_WARNING_THRESHOLD = 75
const DISK_ERROR_THRESHOLD = 90
const BYTE_UNITS = ['B', 'KB', 'MB', 'GB', 'TB'] as const

// Types
type ThreatLevel = 'NONE' | 'INFO' | 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL'

interface MetricsData {
  totalUploaded?: number
  totalDownloaded?: number
  bytesUploaded?: number
  bytesDownloaded?: number
  timestamp?: number
  // File-specific metrics
  filesUploaded?: number
  filesDownloaded?: number
  filesSynced?: number
  totalFileSize?: number
  pendingFiles?: number
  activeTransfers?: number
}

interface HealthData {
  healthy: boolean
  statusMessage: string
  activeWatcherCount?: number
  diskUsagePercent: number
  diskFreeBytes: number
  dbConnected: boolean
  dbSizeBytes: number
}

interface PeerHealthData {
  degraded: boolean
  peerId?: string
}

interface SyncStatusData {
  syncStatus: string
  pendingFiles?: number
  health?: HealthData
  anomaly?: unknown
  peerHealth?: PeerHealthData[]
}

interface ThreatStatusData {
  threatLevel?: ThreatLevel
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

interface NetworkRates {
  upload: number
  download: number
}

interface NetworkPeaks {
  upload: number
  download: number
}

// Helper functions
function formatBytes(bytes: number): string {
  if (bytes === 0) return '0 B'
  const i = Math.floor(Math.log(bytes) / Math.log(BYTES_TO_KB))
  const unitIndex = Math.min(i, BYTE_UNITS.length - 1)
  const value = bytes / Math.pow(BYTES_TO_KB, unitIndex)
  return `${value.toFixed(unitIndex === 0 ? 0 : 2)} ${BYTE_UNITS[unitIndex]}`
}

function getThreatStatusLevel(threatStatus: ThreatStatusData | null): 'error' | 'warning' | 'success' {
  if (!threatStatus?.threatLevel) return 'success'
  const level = threatStatus.threatLevel
  if (level === 'HIGH' || level === 'CRITICAL') return 'error'
  if (level === 'MEDIUM' || level === 'INFO') return 'warning'
  return 'success'
}

function getDiskStatusLevel(diskUsagePercent?: number): 'error' | 'warning' | 'success' {
  if (!diskUsagePercent) return 'success'
  if (diskUsagePercent > DISK_ERROR_THRESHOLD) return 'error'
  if (diskUsagePercent > DISK_WARNING_THRESHOLD) return 'warning'
  return 'success'
}

// Custom hook for network traffic management
function useNetworkTraffic(metrics: MetricsData | null) {
  const [trafficHistory, setTrafficHistory] = useState<TrafficHistoryItem[]>([])
  const [lastMetrics, setLastMetrics] = useState<MetricsData | null>(null)
  const [peaks, setPeaks] = useState<NetworkPeaks>({ upload: 0, download: 0 })
  const [currentRates, setCurrentRates] = useState<NetworkRates>({ upload: 0, download: 0 })

  useEffect(() => {
    if (!metrics) return

    const now = new Date()
    const timeStr = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`

    let uploadRate = 0
    let downloadRate = 0

    if (lastMetrics) {
      const currUp = metrics.totalUploaded ?? metrics.bytesUploaded ?? 0
      const lastUp = lastMetrics.totalUploaded ?? lastMetrics.bytesUploaded ?? 0
      const currDown = metrics.totalDownloaded ?? metrics.bytesDownloaded ?? 0
      const lastDown = lastMetrics.totalDownloaded ?? lastMetrics.bytesDownloaded ?? 0

      // Calculate rates with validation
      if (POLLING_INTERVAL_SECONDS > 0) {
        uploadRate = Math.max(0, (currUp - lastUp) / POLLING_INTERVAL_SECONDS)
        downloadRate = Math.max(0, (currDown - lastDown) / POLLING_INTERVAL_SECONDS)
      }

      setCurrentRates({ upload: uploadRate, download: downloadRate })

      // Update peaks
      setPeaks(prev => ({
        upload: Math.max(prev.upload, uploadRate),
        download: Math.max(prev.download, downloadRate)
      }))
    }

    setLastMetrics(metrics)

    setTrafficHistory(prev => {
      const newHistory = [
        ...prev,
        {
          time: timeStr,
          upload: uploadRate / BYTES_TO_KB,
          download: downloadRate / BYTES_TO_KB,
          uploadBytes: uploadRate,
          downloadBytes: downloadRate
        }
      ]
      return newHistory.slice(-MAX_TRAFFIC_HISTORY_POINTS)
    })
  }, [metrics, lastMetrics])

  return { trafficHistory, currentRates, peaks }
}

// Memoized component for better performance
export const Dashboard = memo(function Dashboard(props: DashboardProps) {
  const {
    metrics = null,
    syncStatus = null,
    peersCount = 0,
    activity = [],
    threatStatus = null,
    onOpenQuarantine
  } = props

  // Use custom hook for network traffic
  const { trafficHistory, currentRates, peaks } = useNetworkTraffic(metrics)
  
  // Use custom hook for file activity metrics
  const fileActivity = useFileActivity(activity)
  
  // Memoized computed values
  const filesSynced = fileActivity.filesSynced
  const totalFileSize = fileActivity.totalFileSize
  const pendingFiles = fileActivity.pendingFiles
  const activeTransfers = fileActivity.activeTransfers
  
  const health = useMemo(() => syncStatus?.health, [syncStatus])
  
  const degradedPeersCount = useMemo(
    () => syncStatus?.peerHealth?.filter(p => p.degraded).length ?? 0,
    [syncStatus?.peerHealth]
  )

  const isThreatDetectionEnabled = useMemo(
    () => Boolean(threatStatus?.enabled),
    [threatStatus]
  )

  const syncStatusValue = useMemo(
    () => syncStatus?.syncStatus === 'ENABLED' ? 'success' as const : 'warning' as const,
    [syncStatus?.syncStatus]
  )

  const diskStatus = useMemo(
    () => getDiskStatusLevel(health?.diskUsagePercent),
    [health?.diskUsagePercent]
  )

  const threatStatusLevel = useMemo(
    () => getThreatStatusLevel(threatStatus),
    [threatStatus]
  )

  return (
    <div className="space-y-4 sm:space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
      {/* Hero Section with Animated Stats */}
      <HeroSection 
        currentUpload={currentRates.upload}
        currentDownload={currentRates.download}
        peakUpload={peaks.upload}
        peakDownload={peaks.download}
        peersCount={peersCount}
        formatBytes={formatBytes}
        filesSynced={filesSynced}
        totalFileSize={totalFileSize}
        pendingFiles={pendingFiles}
        activeTransfers={activeTransfers}
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
          value={syncStatus?.syncStatus ?? "Idle"} 
          sub={syncStatus?.pendingFiles ? `${syncStatus.pendingFiles} files pending` : "Up to date"}
          icon={<Zap className="w-5 h-5 text-primary" />}
          status={syncStatusValue}
        />
        <StatCard 
          title="Active Peers" 
          value={`${peersCount} Device${peersCount !== 1 ? 's' : ''}`} 
          sub="Mesh Swarm"
          icon={<Network className="w-5 h-5 text-primary" />}
        />
        <StatCard 
          title="Files Synced" 
          value={filesSynced.toString()} 
          sub={`Total size: ${formatBytes(totalFileSize)}`}
          icon={<HardDrive className="w-5 h-5 icon-success" />}
          status="success"
        />
      </div>

      {/* Main Content Grid */}
      <div className="grid grid-cols-1 xl:grid-cols-3 gap-4 sm:gap-6">
        {/* Chart Section */}
        <FileActivityChart 
          activityHistory={fileActivity.activityHistory}
          filesSynced={filesSynced}
          filesUploaded={fileActivity.filesUploaded}
          filesDownloaded={fileActivity.filesDownloaded}
          activeTransfers={activeTransfers}
          formatBytes={formatBytes}
        />

        {/* Activity Feed */}
        <ActivityFeed recentActivity={activity} />
      </div>

      {/* Health Summary Row */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3 sm:gap-4">
        <HealthCard
          title="System Health"
          icon={<Shield className="w-5 h-5" />}
          status={health?.healthy ? 'success' : 'warning'}
          value={health?.statusMessage ?? 'Unknown'}
          sub={health ? `${health.activeWatcherCount ?? 0} active watcher${(health.activeWatcherCount ?? 0) !== 1 ? 's' : ''}` : 'Loading...'}
        />
        <HealthCard
          title="Disk Usage"
          icon={<HardDrive className="w-5 h-5" />}
          status={diskStatus}
          value={health ? `${health.diskUsagePercent.toFixed(1)}%` : '--'}
          sub={health ? `${(health.diskFreeBytes / BYTES_TO_GB).toFixed(1)} GB free` : 'Loading...'}
        />
        <HealthCard
          title="Database"
          icon={<Database className="w-5 h-5" />}
          status={health?.dbConnected ? 'success' : 'error'}
          value={health?.dbConnected ? 'Connected' : 'Disconnected'}
          sub={health ? `${(health.dbSizeBytes / BYTES_TO_KB).toFixed(0)} KB` : 'Loading...'}
        />
        <HealthCard
          title="Zer0 Threat Detection"
          icon={<Shield className="w-5 h-5" />}
          status={threatStatusLevel}
          value={isThreatDetectionEnabled ? (threatStatus?.threatLevel === 'NONE' ? 'Safe' : threatStatus?.threatLevel ?? 'Safe') : 'Disabled'}
          sub={isThreatDetectionEnabled ? `${threatStatus?.filesAnalyzed ?? 0} files analyzed` : 'Enable Zer0 plugin'}
        />
      </div>

      {/* Zer0 Threat Detection Panel */}
      <div className="grid grid-cols-1 gap-4 sm:gap-6">
        {isThreatDetectionEnabled && threatStatus && (
          <ThreatAnalysisPanel 
            threatStatus={{
              enabled: isThreatDetectionEnabled,
              threatLevel: threatStatus.threatLevel ?? 'NONE',
              filesAnalyzed: threatStatus.filesAnalyzed ?? 0,
              threatsDetected: threatStatus.threatsDetected ?? threatStatus.totalThreats ?? 0,
              filesQuarantined: threatStatus.filesQuarantined ?? 0,
              hiddenExecutables: threatStatus.hiddenExecutables ?? 0,
              extensionMismatches: threatStatus.extensionMismatches ?? 0,
              ransomwarePatterns: threatStatus.ransomwarePatterns ?? threatStatus.ransomwareAlerts ?? 0,
              behavioralAnomalies: threatStatus.behavioralAnomalies ?? 0,
              magicByteValidation: threatStatus.magicByteValidation ?? true,
              behavioralAnalysis: threatStatus.behavioralAnalysis ?? true,
              fileTypeAwareness: threatStatus.fileTypeAwareness ?? true,
            }} 
            onOpenQuarantine={onOpenQuarantine} 
          />
        )}
      </div>

      {/* Degraded Peers Warning */}
      <DegradedPeersWarning degradedPeers={degradedPeersCount} />
    </div>
  )
})
