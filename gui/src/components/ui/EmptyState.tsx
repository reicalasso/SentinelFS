import React from 'react'

interface EmptyStateProps {
  icon: React.ReactNode
  title: string
  description: string
  action?: React.ReactNode
  variant?: 'default' | 'success' | 'primary' | 'info'
}

export function EmptyState({ icon, title, description, action, variant = 'default' }: EmptyStateProps) {
  return (
    <div className="ui-empty-state">
      <div className={`ui-empty-state-icon-wrapper ui-empty-state-icon-${variant}`}>
        <div className={`ui-empty-state-glow ui-empty-state-glow-${variant}`}></div>
        <div className="ui-empty-state-icon">
          {icon}
        </div>
      </div>
      <h3 className="ui-empty-state-title">{title}</h3>
      <p className="ui-empty-state-description">{description}</p>
      {action && <div className="ui-empty-state-action">{action}</div>}
    </div>
  )
}
