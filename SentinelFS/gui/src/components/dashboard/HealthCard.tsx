export type HealthStatus = 'success' | 'warning' | 'error'

interface HealthCardProps {
  title: string
  icon: React.ReactNode
  status: HealthStatus
  value: string
  sub: string
}

export function HealthCard({ title, icon, status, value, sub }: HealthCardProps) {
  const statusClasses: Record<HealthStatus, string> = {
    success: 'health-card-success',
    warning: 'health-card-warning',
    error: 'health-card-error',
  }
  
  const indicatorClasses: Record<HealthStatus, string> = {
    success: 'health-card-indicator-success',
    warning: 'health-card-indicator-warning',
    error: 'health-card-indicator-error',
  }
  
  return (
    <div className={`health-card ${statusClasses[status]}`}>
      {/* Animated background shimmer */}
      <div className="health-card-shimmer"></div>
      
      <div className="relative">
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center gap-2">
            <div className="health-card-icon">
              {icon}
            </div>
            <span className="text-sm font-semibold text-muted-foreground">{title}</span>
          </div>
          <div className={`health-card-indicator ${indicatorClasses[status]}`}></div>
        </div>
        <div className="text-xl font-bold tracking-tight">{value}</div>
        <div className="health-card-sub">
          <div className="w-1 h-1 rounded-full bg-muted-foreground/50"></div>
          {sub}
        </div>
      </div>
    </div>
  )
}
