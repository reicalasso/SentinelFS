import { Shield, Activity } from 'lucide-react'
import { ActivityItem, ActivityType } from './ActivityItem'

export interface ActivityData {
  type: ActivityType
  file?: string
  path?: string
  time?: string
  timestamp?: number
  details?: string
}

interface ActivityFeedProps {
  recentActivity: ActivityData[]
}

export function ActivityFeed({ recentActivity }: ActivityFeedProps) {
  return (
    <div className="card-modern p-4 sm:p-6 flex flex-col group" style={{ height: 'fit-content' }}>
      {/* Decorative element */}
      <div className="activity-feed-decoration"></div>
      
      <div className="relative flex items-center justify-between mb-4 sm:mb-5">
        <h3 className="activity-feed-title">
          <div className="activity-feed-title-icon">
            <Shield className="w-4 h-4 sm:w-5 sm:h-5 text-accent" />
          </div>
          <span>Live Activity</span>
        </h3>
        <div className="activity-feed-badge">
          <div className="dot-success animate-pulse"></div>
          <span className="hidden sm:inline">Real-time</span>
        </div>
      </div>
      
      {/* Activity List - Fixed height to match chart area */}
      <div className="activity-feed-list custom-scrollbar">
        {recentActivity.length === 0 ? (
          <div className="activity-feed-empty">
            <div className="activity-feed-empty-icon">
              <Activity className="w-10 h-10 text-muted-foreground/30" />
            </div>
            <p className="text-sm text-muted-foreground">No recent activity</p>
            <p className="text-xs text-muted-foreground/60 mt-1">Activity will appear here as files sync</p>
          </div>
        ) : (
          recentActivity.map((item: ActivityData, i: number) => (
            <ActivityItem 
              key={i}
              type={item.type}
              file={item.file ?? item.path ?? 'Unknown'}
              time={item.time ?? new Date(item.timestamp ?? Date.now()).toLocaleTimeString()}
              details={item.details ?? ''}
            />
          ))
        )}
      </div>
    </div>
  )
}
