import React from 'react'
import { Loader2 } from 'lucide-react'

export type ButtonVariant = 'primary' | 'success' | 'warning' | 'error' | 'info' | 'ghost' | 'outline'
export type ButtonSize = 'sm' | 'md' | 'lg'

interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: ButtonVariant
  size?: ButtonSize
  icon?: React.ReactNode
  iconPosition?: 'left' | 'right'
  loading?: boolean
  fullWidth?: boolean
  children?: React.ReactNode
}

export function Button({
  variant = 'primary',
  size = 'md',
  icon,
  iconPosition = 'left',
  loading = false,
  fullWidth = false,
  children,
  className = '',
  disabled,
  ...props
}: ButtonProps) {
  const baseClass = 'ui-btn'
  const variantClass = `ui-btn-${variant}`
  const sizeClass = `ui-btn-${size}`
  const widthClass = fullWidth ? 'w-full' : ''
  const disabledClass = disabled || loading ? 'ui-btn-disabled' : ''
  
  return (
    <button
      className={`${baseClass} ${variantClass} ${sizeClass} ${widthClass} ${disabledClass} ${className}`}
      disabled={disabled || loading}
      {...props}
    >
      {/* Shine effect */}
      <div className="ui-btn-shine"></div>
      
      {loading ? (
        <Loader2 className="w-4 h-4 animate-spin relative" />
      ) : (
        icon && iconPosition === 'left' && <span className="relative">{icon}</span>
      )}
      
      {children && <span className="relative">{children}</span>}
      
      {!loading && icon && iconPosition === 'right' && <span className="relative">{icon}</span>}
    </button>
  )
}
