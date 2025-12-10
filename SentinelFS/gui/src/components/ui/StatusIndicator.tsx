import React from 'react'

export type StatusType = 'online' | 'offline' | 'warning' | 'error' | 'pending'

interface StatusIndicatorProps {
  status: StatusType
  label?: string
  showPulse?: boolean
  size?: 'sm' | 'md' | 'lg'
}

export function StatusIndicator({ status, label, showPulse = true, size = 'md' }: StatusIndicatorProps) {
  const sizeClasses = {
    sm: 'w-2 h-2',
    md: 'w-2.5 h-2.5',
    lg: 'w-3 h-3'
  }
  
  return (
    <div className="ui-status-indicator">
      <div className={`ui-status-dot ui-status-dot-${status} ${sizeClasses[size]} ${showPulse && (status === 'online' || status === 'pending') ? 'animate-pulse' : ''}`}>
        {status === 'online' && showPulse && (
          <div className="ui-status-ping"></div>
        )}
      </div>
      {label && <span className={`ui-status-label ui-status-label-${status}`}>{label}</span>}
    </div>
  )
}
