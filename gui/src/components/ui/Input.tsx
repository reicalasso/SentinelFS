import React from 'react'

interface InputProps extends React.InputHTMLAttributes<HTMLInputElement> {
  icon?: React.ReactNode
  error?: string
  label?: string
}

export function Input({ 
  icon, 
  error, 
  label,
  className = '', 
  ...props 
}: InputProps) {
  return (
    <div className="ui-input-wrapper">
      {label && <label className="ui-input-label">{label}</label>}
      <div className={`ui-input-container ${error ? 'ui-input-error' : ''}`}>
        {icon && <span className="ui-input-icon">{icon}</span>}
        <input 
          className={`ui-input ${icon ? 'ui-input-with-icon' : ''} ${className}`}
          {...props}
        />
      </div>
      {error && <span className="ui-input-error-text">{error}</span>}
    </div>
  )
}

interface SelectProps extends React.SelectHTMLAttributes<HTMLSelectElement> {
  options: { value: string; label: string }[]
  label?: string
}

export function Select({ options, label, className = '', ...props }: SelectProps) {
  return (
    <div className="ui-input-wrapper">
      {label && <label className="ui-input-label">{label}</label>}
      <select className={`ui-select ${className}`} {...props}>
        {options.map(opt => (
          <option key={opt.value} value={opt.value}>{opt.label}</option>
        ))}
      </select>
    </div>
  )
}
