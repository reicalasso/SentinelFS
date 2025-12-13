import { ArrowRightLeft, TrendingUp, Zap, RefreshCw, Download, Upload, Activity, Clock, CheckCircle2 } from 'lucide-react'
import { memo } from 'react'
import { TransfersStats, TransferCard, EnhancedTransferCard, TransfersHistory, TransfersEmpty } from './transfers'

interface ActivityItem {
  type: string
  file: string
  time: string
  details: string
}

interface TransfersProps {
  metrics?: any
  transfers?: any[]
  history?: any[]
  activity?: ActivityItem[]
  onPauseTransfer?: (id: string) => void
  onResumeTransfer?: (id: string) => void
  onCancelTransfer?: (id: string) => void
  onRetryTransfer?: (id: string) => void
  onViewTransfer?: (transfer: any) => void
}

export const Transfers = memo(function Transfers({ 
  metrics, 
  transfers, 
  history, 
  activity,
  onPauseTransfer,
  onResumeTransfer,
  onCancelTransfer,
  onRetryTransfer,
  onViewTransfer
}: TransfersProps) {
  const formatSize = (bytes: number) => {
    if (!bytes) return '0 B'
    if (bytes < 1024) return bytes + ' B'
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  }
  
  const totalUploaded = metrics?.totalUploaded || 0
  const totalDownloaded = metrics?.totalDownloaded || 0
  const filesSynced = metrics?.filesSynced || 0
  
  // Active transfers from daemon
  const activeTransfers = transfers && transfers.length > 0 ? transfers : []
  
  // Convert activity to history format if history is empty
  // Activity items from dashboard can be used as fallback history
  const activityAsHistory = (activity || []).slice(0, 20).map((item: ActivityItem) => ({
    file: item.file,
    type: item.details?.includes('synced') ? 'sync' : 'upload',
    time: item.time,
    size: ''
  }))
  
  // Use history from daemon, or fallback to activity
  const displayHistory = (history && history.length > 0) ? history : activityAsHistory

  return (
    <div className="space-y-4 sm:space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
      {/* Hero Section */}
      <div className="transfers-hero">
        {/* Animated Background */}
        <div className="absolute inset-0 overflow-hidden">
          <div className="transfers-hero-orb transfers-hero-orb-1"></div>
          <div className="transfers-hero-orb transfers-hero-orb-2"></div>
          <div className="transfers-hero-pattern"></div>
        </div>
        
        <div className="relative z-10">
          <div className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-4 mb-4 sm:mb-6">
            <div className="flex items-center gap-3 sm:gap-4">
              <div className="transfers-hero-icon">
                <ArrowRightLeft className="w-6 h-6 sm:w-8 sm:h-8" />
              </div>
              <div>
                <h2 className="transfers-hero-title">Transfer Activity</h2>
                <p className="transfers-hero-subtitle">
                  <span className="transfers-hero-status-dot"></span>
                  Real-time sync monitoring
                </p>
              </div>
            </div>
            
            <div className="flex gap-2">
              <button className="transfers-refresh-btn">
                <RefreshCw className="w-4 h-4" />
                <span className="hidden sm:inline">Refresh</span>
              </button>
            </div>
          </div>
          
          {/* Quick Stats in Hero */}
          <div className="grid grid-cols-3 gap-2 sm:gap-4">
            <div className="transfers-hero-stat transfers-hero-stat-download">
              <div className="transfers-hero-stat-icon">
                <Download className="w-4 h-4 sm:w-5 sm:h-5" />
              </div>
              <div className="min-w-0">
                <div className="transfers-hero-stat-value">{formatSize(totalDownloaded)}</div>
                <div className="transfers-hero-stat-label">Downloaded</div>
              </div>
            </div>
            <div className="transfers-hero-stat transfers-hero-stat-upload">
              <div className="transfers-hero-stat-icon">
                <Upload className="w-4 h-4 sm:w-5 sm:h-5" />
              </div>
              <div className="min-w-0">
                <div className="transfers-hero-stat-value">{formatSize(totalUploaded)}</div>
                <div className="transfers-hero-stat-label">Uploaded</div>
              </div>
            </div>
            <div className="transfers-hero-stat transfers-hero-stat-synced">
              <div className="transfers-hero-stat-icon">
                <Activity className="w-4 h-4 sm:w-5 sm:h-5" />
              </div>
              <div className="min-w-0">
                <div className="transfers-hero-stat-value">{filesSynced}</div>
                <div className="transfers-hero-stat-label">Synced</div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Active Transfers Section */}
      <div className="transfers-section">
        <div className="transfers-section-header">
          <div className="transfers-section-title-wrapper">
            <div className="transfers-section-icon transfers-section-icon-active">
              <Zap className="w-4 h-4" />
            </div>
            <div>
              <h3 className="transfers-section-title">Active Transfers</h3>
              <p className="transfers-section-count">{activeTransfers.length} in progress</p>
            </div>
          </div>
          {activeTransfers.length > 0 && (
            <div className="transfers-active-indicator">
              <span className="transfers-active-dot"></span>
              <span className="text-xs font-medium">Live</span>
            </div>
          )}
        </div>
        
        <div className="transfers-section-content">
          {activeTransfers.length === 0 ? (
            <TransfersEmpty />
          ) : (
            <div className="space-y-3">
              {activeTransfers.map((transfer: any, i: number) => (
                <EnhancedTransferCard 
                  key={transfer.id || i} 
                  transfer={transfer}
                  onPause={onPauseTransfer}
                  onResume={onResumeTransfer}
                  onCancel={onCancelTransfer}
                  onRetry={onRetryTransfer}
                  onView={onViewTransfer}
                />
              ))}
            </div>
          )}
        </div>
      </div>

      {/* History Section */}
      <div className="transfers-section">
        <div className="transfers-section-header">
          <div className="transfers-section-title-wrapper">
            <div className="transfers-section-icon transfers-section-icon-history">
              <Clock className="w-4 h-4" />
            </div>
            <div>
              <h3 className="transfers-section-title">Recent History</h3>
              <p className="transfers-section-count">{displayHistory.length} recent transfers</p>
            </div>
          </div>
        </div>
        
        <div className="transfers-section-content">
          <TransfersHistory history={displayHistory} />
        </div>
      </div>
    </div>
  )
})