import { Download, Upload, Activity } from 'lucide-react'

interface TransfersStatsProps {
  totalDownloaded: number
  totalUploaded: number
  filesSynced: number
  formatSize: (bytes: number) => string
}

export function TransfersStats({ totalDownloaded, totalUploaded, filesSynced, formatSize }: TransfersStatsProps) {
  return (
    <div className="transfers-stats-grid">
      <div className="transfers-stat-card transfers-stat-download">
        <div className="transfers-stat-icon transfers-stat-icon-download">
          <Download className="w-5 h-5 sm:w-6 sm:h-6" />
        </div>
        <div>
          <p className="transfers-stat-label">Total Downloaded</p>
          <p className="transfers-stat-value">{formatSize(totalDownloaded)}</p>
        </div>
      </div>
      <div className="transfers-stat-card transfers-stat-upload">
        <div className="transfers-stat-icon transfers-stat-icon-upload">
          <Upload className="w-5 h-5 sm:w-6 sm:h-6" />
        </div>
        <div>
          <p className="transfers-stat-label">Total Uploaded</p>
          <p className="transfers-stat-value">{formatSize(totalUploaded)}</p>
        </div>
      </div>
      <div className="transfers-stat-card transfers-stat-synced">
        <div className="transfers-stat-icon transfers-stat-icon-synced">
          <Activity className="w-5 h-5 sm:w-6 sm:h-6" />
        </div>
        <div>
          <p className="transfers-stat-label">Files Synced</p>
          <p className="transfers-stat-value">{filesSynced}</p>
        </div>
      </div>
    </div>
  )
}
