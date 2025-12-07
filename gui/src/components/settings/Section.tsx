interface SectionProps {
  title: string
  children: React.ReactNode
}

export function Section({ title, children }: SectionProps) {
  return (
    <div className="settings-section">
      <h3 className="settings-section-title">
        <div className="settings-section-accent"></div>
        {title}
      </h3>
      {children}
    </div>
  )
}
