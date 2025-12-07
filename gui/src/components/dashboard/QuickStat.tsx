export type QuickStatColor = 'teal' | 'emerald' | 'coral' | 'amber'

interface QuickStatProps {
  label: string
  value: string | number
  sub?: string
  icon: React.ReactNode
  color: QuickStatColor
}

export function QuickStat({ label, value, sub, icon, color }: QuickStatProps) {
  const colorClasses: Record<QuickStatColor, string> = {
    teal: 'quick-stat-teal',
    emerald: 'quick-stat-emerald',
    coral: 'quick-stat-coral',
    amber: 'quick-stat-amber',
  }
  
  return (
    <div className={`quick-stat ${colorClasses[color]}`}>
      <div className="quick-stat-icon">{icon}</div>
      <div className="min-w-0">
        <div className="quick-stat-value">{value}</div>
        <div className="quick-stat-label">{label}</div>
        {sub && <div className="quick-stat-sub">{sub}</div>}
      </div>
    </div>
  )
}
