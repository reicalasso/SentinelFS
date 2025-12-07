import { ArrowDown, ArrowUp, Shield, Activity } from 'lucide-react'

export type ActivityType = 'sync' | 'delete' | 'security' | string

interface ActivityItemProps {
  type: ActivityType
  file: string
  time: string
  details: string
}

export function ActivityItem({ type, file, time, details }: ActivityItemProps) {
  const getTypeStyles = () => {
    switch(type) {
      case 'sync': return { 
        icon: <ArrowDown className="w-4 h-4" />, 
        className: 'activity-item-sync'
      }
      case 'delete': return { 
        icon: <ArrowUp className="w-4 h-4" />, 
        className: 'activity-item-delete'
      }
      case 'security': return { 
        icon: <Shield className="w-4 h-4" />, 
        className: 'activity-item-security'
      }
      default: return { 
        icon: <Activity className="w-4 h-4" />, 
        className: 'activity-item-default'
      }
    }
  }
  
  const styles = getTypeStyles()

  return (
    <div className="activity-item">
      <div className={`activity-item-icon ${styles.className}`}>
        {styles.icon}
      </div>
      <div className="flex-1 min-w-0">
        <div className="flex items-center justify-between gap-2">
          <div className="activity-item-file">{file}</div>
          <div className="activity-item-time">{time}</div>
        </div>
        <div className="activity-item-details">{details}</div>
      </div>
    </div>
  )
}
