import React from 'react'

export type PageHeaderVariant = 'primary' | 'success' | 'info' | 'accent'

interface PageHeaderProps {
  icon: React.ReactNode
  title: string
  subtitle: string
  statusText?: string
  variant?: PageHeaderVariant
  actions?: React.ReactNode
  stats?: React.ReactNode
}

export function PageHeader({ 
  icon, 
  title, 
  subtitle, 
  statusText,
  variant = 'primary',
  actions,
  stats
}: PageHeaderProps) {
  return (
    <div className={`page-header page-header-${variant}`}>
      {/* Animated Background */}
      <div className="absolute inset-0 overflow-hidden">
        <div className={`page-header-bg-1 page-header-bg-1-${variant}`}></div>
        <div className={`page-header-bg-2 page-header-bg-2-${variant}`}></div>
        <div className="page-header-pattern"></div>
      </div>
      
      <div className="relative z-10">
        <div className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-4 mb-4 sm:mb-6">
          <div className="flex items-center gap-3 sm:gap-4">
            <div className={`page-header-icon page-header-icon-${variant}`}>
              {icon}
            </div>
            <div>
              <h2 className="page-header-title">{title}</h2>
              <p className="page-header-subtitle">
                {statusText && <span className={`dot-${variant === 'success' ? 'success' : variant === 'info' ? 'info' : 'primary'} animate-pulse`}></span>}
                {subtitle}
              </p>
            </div>
          </div>
          
          {actions && (
            <div className="flex flex-wrap gap-2 sm:gap-3">
              {actions}
            </div>
          )}
        </div>
        
        {stats && (
          <div className="grid grid-cols-3 gap-2 sm:gap-4">
            {stats}
          </div>
        )}
      </div>
    </div>
  )
}

interface PageHeaderStatProps {
  icon: React.ReactNode
  value: string | number
  label: string
  variant?: PageHeaderVariant
}

export function PageHeaderStat({ icon, value, label, variant = 'primary' }: PageHeaderStatProps) {
  return (
    <div className={`page-header-stat page-header-stat-${variant}`}>
      <div className="page-header-stat-icon">{icon}</div>
      <div className="min-w-0">
        <div className={`page-header-stat-value page-header-stat-value-${variant}`}>{value}</div>
        <div className="page-header-stat-label">{label}</div>
      </div>
    </div>
  )
}
