import { ArrowDown, ArrowUp, Pause, X, Play, Zap } from 'lucide-react'

interface TransferCardProps {
  transfer: any
}

export function TransferCard({ transfer }: TransferCardProps) {
  const isDownload = transfer.type === 'download'
  
  return (
    <div className={`transfer-card-modern ${isDownload ? 'transfer-card-download' : 'transfer-card-upload'}`}>
      {/* Progress Background */}
      <div 
        className={`transfer-card-progress-bg ${isDownload ? 'transfer-card-progress-bg-download' : 'transfer-card-progress-bg-upload'}`}
        style={{ width: `${transfer.progress}%` }}
      ></div>
      
      <div className="transfer-card-content">
        <div className="transfer-card-left">
          <div className={`transfer-card-icon-modern ${isDownload ? 'transfer-card-icon-download' : 'transfer-card-icon-upload'}`}>
            {isDownload ? <ArrowDown className="w-5 h-5" /> : <ArrowUp className="w-5 h-5" />}
          </div>
          <div className="transfer-card-info-modern">
            <div className="transfer-card-filename-modern">{transfer.file}</div>
            <div className="transfer-card-meta-modern">
              <span className="transfer-card-peer">{isDownload ? 'From' : 'To'} {transfer.peer}</span>
              <span className="transfer-card-dot">•</span>
              <span className="transfer-card-size">{transfer.size}</span>
            </div>
          </div>
        </div>
        
        <div className="transfer-card-right">
          <div className="transfer-card-stats">
            <div className="transfer-card-speed-modern">
              <Zap className="w-3 h-3" />
              {transfer.speed}
            </div>
            <div className="transfer-card-progress-text-modern">{transfer.progress}%</div>
          </div>
          <div className="transfer-card-actions-modern">
            <button className="transfer-card-action-btn transfer-card-action-pause">
              <Pause className="w-4 h-4" />
            </button>
            <button className="transfer-card-action-btn transfer-card-action-cancel">
              <X className="w-4 h-4" />
            </button>
          </div>
        </div>
      </div>
      
      {/* Progress Bar */}
      <div className="transfer-card-progress-bar-modern">
        <div 
          className={`transfer-card-progress-fill-modern ${isDownload ? 'transfer-progress-download' : 'transfer-progress-upload'}`}
          style={{ width: `${transfer.progress}%` }}
        ></div>
      </div>
    </div>
  )
}
