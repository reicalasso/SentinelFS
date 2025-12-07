import React from 'react'

export type CardVariant = 'default' | 'success' | 'warning' | 'error' | 'info' | 'primary'

interface CardProps {
  children: React.ReactNode
  variant?: CardVariant
  className?: string
  padding?: 'none' | 'sm' | 'md' | 'lg'
  hover?: boolean
}

export function Card({ 
  children, 
  variant = 'default', 
  className = '',
  padding = 'md',
  hover = true 
}: CardProps) {
  const paddingClasses = {
    none: '',
    sm: 'p-3 sm:p-4',
    md: 'p-4 sm:p-5',
    lg: 'p-5 sm:p-6'
  }
  
  return (
    <div className={`ui-card ui-card-${variant} ${paddingClasses[padding]} ${hover ? 'ui-card-hover' : ''} ${className}`}>
      {children}
    </div>
  )
}

interface CardHeaderProps {
  icon?: React.ReactNode
  title: string
  subtitle?: string
  action?: React.ReactNode
}

export function CardHeader({ icon, title, subtitle, action }: CardHeaderProps) {
  return (
    <div className="ui-card-header">
      <div className="flex items-center gap-3">
        {icon && <div className="ui-card-header-icon">{icon}</div>}
        <div>
          <h3 className="ui-card-header-title">{title}</h3>
          {subtitle && <p className="ui-card-header-subtitle">{subtitle}</p>}
        </div>
      </div>
      {action && <div>{action}</div>}
    </div>
  )
}
