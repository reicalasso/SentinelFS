interface ToggleProps {
  checked: boolean
  onChange: () => void
}

export function Toggle({ checked, onChange }: ToggleProps) {
  return (
    <div 
      onClick={onChange}
      className={`settings-toggle ${checked ? 'settings-toggle-on' : 'settings-toggle-off'}`}
    >
      <div className={`settings-toggle-knob ${checked ? 'right-1' : 'left-1'}`}></div>
    </div>
  )
}
