export type ThreatStatus = 'success' | 'warning' | 'error'

interface ThreatStatCardProps {
  icon: React.ReactNode
  label: string
  value: number | string
  status: ThreatStatus
}

export function ThreatStatCard({ icon, label, value, status }: ThreatStatCardProps) {
  const statusClasses: Record<ThreatStatus, string> = {
    success: 'threat-stat-success',
    warning: 'threat-stat-warning',
    error: 'threat-stat-error'
  }
  
  return (
    <div className={`threat-stat-card ${statusClasses[status]}`}>
      <div className="flex items-center gap-2 mb-1">
        <span className="threat-stat-icon">{icon}</span>
        <span className="threat-stat-label">{label}</span>
      </div>
      <div className="threat-stat-value">{value}</div>
    </div>
  )
}
