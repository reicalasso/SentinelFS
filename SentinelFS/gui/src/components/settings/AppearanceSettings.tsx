import { Moon, Sun, Check } from 'lucide-react'
import { useTheme } from '../../context/ThemeContext'
import { useAppearance, themes, type FontId } from '../../context/AppearanceContext'
import { Section } from './Section'
import { Toggle } from './Toggle'

const localFonts = [
  { id: 'pixellet' as FontId, name: 'Pixellet', description: 'Retro pixel style', family: "'Pixellet', system-ui, sans-serif", preview: 'SentinelFS' },
  { id: 'plank' as FontId, name: 'Plank', description: 'Bold display font', family: "'Plank', system-ui, sans-serif", preview: 'SentinelFS' },
  { id: 'super-croissant' as FontId, name: 'Super Croissant', description: 'Playful rounded font', family: "'Super Croissant', system-ui, sans-serif", preview: 'SentinelFS' },
  { id: 'spicy-sale' as FontId, name: 'Spicy Sale', description: 'Fun decorative font', family: "'Spicy Sale', system-ui, sans-serif", preview: 'SentinelFS' },
  { id: 'star' as FontId, name: 'Star', description: 'Starry display font', family: "'Star', system-ui, sans-serif", preview: 'SentinelFS' },
  { id: 'playful-christmas' as FontId, name: 'Playful Christmas', description: 'Festive holiday font', family: "'Playful Christmas', system-ui, sans-serif", preview: 'SentinelFS' }
]

const webFonts = [
  { id: 'inter' as FontId, name: 'Inter', description: 'Modern sans-serif', family: "'Inter', system-ui, sans-serif", preview: 'SentinelFS' },
  { id: 'jetbrains' as FontId, name: 'JetBrains Mono', description: 'Developer monospace', family: "'JetBrains Mono', monospace", preview: 'SentinelFS' },
  { id: 'fira-code' as FontId, name: 'Fira Code', description: 'Code with ligatures', family: "'Fira Code', monospace", preview: 'SentinelFS' },
  { id: 'roboto-mono' as FontId, name: 'Roboto Mono', description: 'Clean monospace', family: "'Roboto Mono', monospace", preview: 'SentinelFS' },
  { id: 'system' as FontId, name: 'System Default', description: 'Use system fonts', family: "system-ui, -apple-system, sans-serif", preview: 'SentinelFS' }
]

const allFonts = [...localFonts, ...webFonts]

export function AppearanceSettings() {
  const { settings, setTheme, setFont, setColorMode } = useAppearance()
  const { theme, toggleTheme } = useTheme()

  const getThemeGradient = (themeColors: any, isDark: boolean) => {
    const colors = isDark ? themeColors.dark : themeColors.light
    return `linear-gradient(135deg, hsl(${colors.primary}) 0%, hsl(${colors.accent}) 100%)`
  }

  return (
    <div className="space-y-6">
      {/* Dark/Light Mode Toggle */}
      <Section title="Color Mode">
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-3">
            {theme === 'dark' ? <Moon className="w-5 h-5 status-info" /> : <Sun className="w-5 h-5 text-primary" />}
            <div>
              <span className="text-sm font-medium block">Theme Mode</span>
              <span className="text-xs text-muted-foreground">{theme === 'dark' ? 'Dark Mode' : 'Light Mode'}</span>
            </div>
          </div>
          <Toggle checked={theme === 'dark'} onChange={() => {
            toggleTheme()
            setColorMode(theme === 'dark' ? 'light' : 'dark')
          }} />
        </div>
      </Section>

      {/* Theme Selection */}
      <Section title="Color Theme">
        <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
          {themes.map((t) => (
            <button
              key={t.id}
              onClick={() => setTheme(t.id)}
              className={`group settings-theme-btn ${settings.themeId === t.id ? 'settings-theme-btn-active' : ''}`}
            >
              <div 
                className="settings-theme-preview"
                style={{ background: getThemeGradient(t.colors, theme === 'dark') }}
              />
              <div className="flex-1 text-left">
                <div className="flex items-center gap-2">
                  <span className="text-sm font-semibold">{t.name}</span>
                  {settings.themeId === t.id && <Check className="w-3.5 h-3.5 text-primary" />}
                </div>
                <span className="text-xs text-muted-foreground">{t.description}</span>
              </div>
            </button>
          ))}
        </div>
      </Section>

      {/* Font Selection */}
      <Section title="Font Family">
        <div className="grid grid-cols-1 gap-2">
          {allFonts.map((f) => (
            <button
              key={f.id}
              onClick={() => setFont(f.id)}
              className={`group settings-font-btn ${settings.fontId === f.id ? 'settings-font-btn-active' : ''}`}
            >
              <div className="flex items-center gap-4">
                <div className="w-28 text-lg font-medium font-preview" style={{ '--preview-font': f.family } as React.CSSProperties}>
                  {f.preview}
                </div>
                <div className="text-left">
                  <div className="flex items-center gap-2">
                    <span className="text-sm font-semibold">{f.name}</span>
                    {settings.fontId === f.id && <Check className="w-3.5 h-3.5 text-primary" />}
                  </div>
                  <span className="text-xs text-muted-foreground">{f.description}</span>
                </div>
              </div>
              <div className={`settings-font-radio ${settings.fontId === f.id ? 'settings-font-radio-active' : ''}`}>
                {settings.fontId === f.id && <div className="settings-font-radio-dot" />}
              </div>
            </button>
          ))}
        </div>
      </Section>
    </div>
  )
}
