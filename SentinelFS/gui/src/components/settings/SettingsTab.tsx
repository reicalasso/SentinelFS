interface SettingsTabProps {
  active: boolean
  onClick: () => void
  icon: React.ReactNode
  label: string
}

export function SettingsTab({ active, onClick, icon, label }: SettingsTabProps) {
  return (
    <button 
      onClick={onClick}
      className={`settings-tab ${active ? 'settings-tab-active' : 'settings-tab-inactive'}`}
    >
      <span className={`transition-colors ${active ? 'text-primary' : ''}`}>{icon}</span>
      {label}
      {active && <div className="settings-tab-indicator"></div>}
    </button>
  )
}
