import { CheckCircle2, CloudOff, Sparkles } from 'lucide-react'

export function TransfersEmpty() {
  return (
    <div className="transfers-empty-modern">
      <div className="transfers-empty-icon-wrapper">
        <div className="transfers-empty-glow"></div>
        <div className="transfers-empty-icon">
          <CheckCircle2 className="w-8 h-8" />
        </div>
      </div>
      <div className="transfers-empty-content">
        <h4 className="transfers-empty-title">All Synced!</h4>
        <p className="transfers-empty-desc">No active transfers. Your files are up to date.</p>
      </div>
      <div className="transfers-empty-sparkles">
        <Sparkles className="w-3 h-3" />
      </div>
    </div>
  )
}
