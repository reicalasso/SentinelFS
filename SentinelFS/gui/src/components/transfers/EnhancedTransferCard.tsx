import { memo, useState, useCallback } from 'react'
import { Pause, Play, X, Download, Upload, AlertCircle, CheckCircle2, Clock, FileText, MoreHorizontal, RefreshCw } from 'lucide-react'
import { formatBytes } from '../utils/format'

interface Transfer {
  id: string
  type: 'upload' | 'download' | 'sync'
  file: string
  path: string
  size: number
  transferred: number
  speed: number
  status: 'pending' | 'active' | 'paused' | 'completed' | 'error'
  error?: string
  startTime?: number
  eta?: number
  peer?: string
}

interface EnhancedTransferCardProps {
  transfer: Transfer
  onPause?: (id: string) => void
  onResume?: (id: string) => void
  onCancel?: (id: string) => void
  onRetry?: (id: string) => void
  onView?: (transfer: Transfer) => void
}

export const EnhancedTransferCard = memo(function EnhancedTransferCard({
  transfer,
  onPause,
  onResume,
  onCancel,
  onRetry,
  onView
}: EnhancedTransferCardProps) {
  const [showMenu, setShowMenu] = useState(false)
  
  const progress = transfer.size > 0 ? (transfer.transferred / transfer.size) * 100 : 0
  const isCompleted = transfer.status === 'completed'
  const hasError = transfer.status === 'error'
  const isActive = transfer.status === 'active'
  const isPaused = transfer.status === 'paused'
  const isPending = transfer.status === 'pending'
  
  const getIcon = () => {
    if (hasError) return <AlertCircle className="w-4 h-4 text-red-500" />
    if (isCompleted) return <CheckCircle2 className="w-4 h-4 text-green-500" />
    if (transfer.type === 'upload') return <Upload className="w-4 h-4 text-blue-500" />
    if (transfer.type === 'download') return <Download className="w-4 h-4 text-green-500" />
    return <FileText className="w-4 h-4 text-muted-foreground" />
  }
  
  const getStatusText = () => {
    if (hasError) return 'Failed'
    if (isCompleted) return 'Completed'
    if (isPaused) return 'Paused'
    if (isActive) return 'Transferring'
    if (isPending) return 'Queued'
    return 'Unknown'
  }
  
  const formatSpeed = () => {
    if (!transfer.speed || transfer.speed === 0) return ''
    return `${formatBytes(transfer.speed)}/s`
  }
  
  const formatETA = () => {
    if (!transfer.eta || transfer.eta === 0) return ''
    const minutes = Math.floor(transfer.eta / 60)
    const seconds = Math.floor(transfer.eta % 60)
    if (minutes > 0) return `${minutes}m ${seconds}s`
    return `${seconds}s`
  }
  
  const handleAction = useCallback((action: () => void) => {
    action()
    setShowMenu(false)
  }, [])
  
  return (
    <div className="group relative p-4 rounded-lg border bg-card hover:bg-accent/50 transition-all">
      {/* Header */}
      <div className="flex items-start justify-between mb-3">
        <div className="flex items-start gap-3 flex-1 min-w-0">
          {getIcon()}
          <div className="flex-1 min-w-0">
            <h4 className="font-medium truncate" title={transfer.file}>
              {transfer.file}
            </h4>
            <p className="text-xs text-muted-foreground truncate" title={transfer.path}>
              {transfer.path}
            </p>
            {transfer.peer && (
              <p className="text-xs text-muted-foreground">
                Peer: {transfer.peer}
              </p>
            )}
          </div>
        </div>
        
        {/* Status Badge */}
        <div className="flex items-center gap-2">
          <span className={`px-2 py-1 text-xs rounded-full font-medium ${
            hasError ? 'bg-red-100 text-red-700 dark:bg-red-900/20 dark:text-red-400' :
            isCompleted ? 'bg-green-100 text-green-700 dark:bg-green-900/20 dark:text-green-400' :
            isPaused ? 'bg-yellow-100 text-yellow-700 dark:bg-yellow-900/20 dark:text-yellow-400' :
            isActive ? 'bg-blue-100 text-blue-700 dark:bg-blue-900/20 dark:text-blue-400' :
            'bg-gray-100 text-gray-700 dark:bg-gray-900/20 dark:text-gray-400'
          }`}>
            {getStatusText()}
          </span>
          
          {/* Action Menu */}
          <div className="relative">
            <button
              onClick={() => setShowMenu(!showMenu)}
              className="p-1 rounded hover:bg-accent transition-colors"
            >
              <MoreHorizontal className="w-4 h-4" />
            </button>
            
            {showMenu && (
              <div className="absolute right-0 top-full mt-1 w-48 rounded-md border bg-popover shadow-md z-50">
                <div className="p-1">
                  {onView && (
                    <button
                      onClick={() => handleAction(() => onView?.(transfer))}
                      className="w-full px-3 py-2 text-sm text-left hover:bg-accent rounded flex items-center gap-2"
                    >
                      <FileText className="w-4 h-4" />
                      View Details
                    </button>
                  )}
                  
                  {isActive && onPause && (
                    <button
                      onClick={() => handleAction(() => onPause?.(transfer.id))}
                      className="w-full px-3 py-2 text-sm text-left hover:bg-accent rounded flex items-center gap-2"
                    >
                      <Pause className="w-4 h-4" />
                      Pause
                    </button>
                  )}
                  
                  {(isPaused || isPending) && onResume && (
                    <button
                      onClick={() => handleAction(() => onResume?.(transfer.id))}
                      className="w-full px-3 py-2 text-sm text-left hover:bg-accent rounded flex items-center gap-2"
                    >
                      <Play className="w-4 h-4" />
                      Resume
                    </button>
                  )}
                  
                  {hasError && onRetry && (
                    <button
                      onClick={() => handleAction(() => onRetry?.(transfer.id))}
                      className="w-full px-3 py-2 text-sm text-left hover:bg-accent rounded flex items-center gap-2"
                    >
                      <RefreshCw className="w-4 h-4" />
                      Retry
                    </button>
                  )}
                  
                  {!isCompleted && onCancel && (
                    <button
                      onClick={() => handleAction(() => onCancel?.(transfer.id))}
                      className="w-full px-3 py-2 text-sm text-left hover:bg-accent rounded flex items-center gap-2 text-red-600"
                    >
                      <X className="w-4 h-4" />
                      Cancel
                    </button>
                  )}
                </div>
              </div>
            )}
          </div>
        </div>
      </div>
      
      {/* Progress Bar */}
      <div className="space-y-2">
        <div className="flex items-center justify-between text-xs text-muted-foreground">
          <span>{formatBytes(transfer.transferred)} / {formatBytes(transfer.size)}</span>
          <div className="flex items-center gap-2">
            {formatSpeed() && <span>{formatSpeed()}</span>}
            {formatETA() && <span className="flex items-center gap-1"><Clock className="w-3 h-3" />{formatETA()}</span>}
          </div>
        </div>
        
        <div className="w-full h-2 bg-secondary rounded-full overflow-hidden">
          <div 
            className={`h-full transition-all duration-300 ${
              hasError ? 'bg-red-500' :
              isCompleted ? 'bg-green-500' :
              isPaused ? 'bg-yellow-500' :
              'bg-blue-500'
            }`}
            style={{ width: `${Math.min(progress, 100)}%` }}
          />
        </div>
        
        {/* Progress Percentage */}
        <div className="text-right">
          <span className="text-xs font-medium">{Math.round(progress)}%</span>
        </div>
      </div>
      
      {/* Error Message */}
      {hasError && transfer.error && (
        <div className="mt-2 p-2 rounded bg-red-50 dark:bg-red-900/20 border border-red-200 dark:border-red-800">
          <p className="text-xs text-red-700 dark:text-red-400">{transfer.error}</p>
        </div>
      )}
    </div>
  )
})
