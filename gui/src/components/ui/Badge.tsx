import React from 'react'

export type BadgeVariant = 'success' | 'warning' | 'error' | 'info' | 'primary' | 'default'
export type BadgeSize = 'sm' | 'md'

interface BadgeProps {
  children: React.ReactNode
  variant?: BadgeVariant
  size?: BadgeSize
  dot?: boolean
  pulse?: boolean
  className?: string
}

export function Badge({ 
  children, 
  variant = 'default', 
  size = 'md',
  dot = false,
  pulse = false,
  className = '' 
}: BadgeProps) {
  return (
    <span className={`ui-badge ui-badge-${variant} ui-badge-${size} ${className}`}>
      {dot && <span className={`ui-badge-dot ui-badge-dot-${variant} ${pulse ? 'animate-pulse' : ''}`}></span>}
      {children}
    </span>
  )
}
