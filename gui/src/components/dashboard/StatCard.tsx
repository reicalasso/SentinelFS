import { Sparkles } from 'lucide-react'

interface StatCardProps {
  title: string
  value: string
  sub: string
  icon: React.ReactNode
  status?: 'success' | 'warning'
  trend?: string
}

export function StatCard({ title, value, sub, icon, status, trend }: StatCardProps) {
  return (
    <div className="card-modern p-5 group">
      {/* Animated corner accent */}
      <div className="stat-card-corner-accent"></div>
      
      <div className="relative flex justify-between items-start mb-4">
        <div className="stat-card-icon-wrapper">
          {icon}
        </div>
        {status && (
          <div className={`stat-card-status ${status === 'success' ? 'stat-card-status-success' : 'stat-card-status-warning'}`}>
            <div className={`stat-card-status-dot ${status === 'success' ? 'stat-card-status-dot-success' : 'stat-card-status-dot-warning'}`} />
            {status === 'success' ? 'Online' : 'Warning'}
          </div>
        )}
      </div>
      
      <div className="relative">
        <h3 className="stat-card-title">{title}</h3>
        <div className="stat-card-value">{value}</div>
        <p className="stat-card-sub">
          <Sparkles className="w-3 h-3 text-primary/50" />
          {sub}
        </p>
      </div>
      
      {/* Bottom accent line */}
      <div className="stat-card-bottom-accent"></div>
    </div>
  )
}
