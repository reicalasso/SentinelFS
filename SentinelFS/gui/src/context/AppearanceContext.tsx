import { createContext, useContext, useState, useEffect, ReactNode, useCallback } from 'react'

// ==========================================
// THEME DEFINITIONS
// ==========================================

export type ThemeId = 'ocean-sunset' | 'midnight-blue' | 'forest-green' | 'purple-haze' | 'monochrome' | 'forward-to-past'

export interface ThemeColors {
  primary: string       // Main accent color (HSL values)
  secondary: string     // Secondary color
  accent: string        // Accent highlights
  success: string       // Success states
  warning: string       // Warning states
  error: string         // Error states
  info: string          // Info states
  background: string    // Background color
  foreground: string    // Text color
  card: string          // Card background
  border: string        // Border color
  muted: string         // Muted text
}

export interface Theme {
  id: ThemeId
  name: string
  description: string
  colors: {
    dark: ThemeColors
    light: ThemeColors
  }
}

// Ocean Sunset - Current Default
const oceanSunset: Theme = {
  id: 'ocean-sunset',
  name: 'Ocean Sunset',
  description: 'Warm teal and amber tones',
  colors: {
    dark: {
      primary: '30 100% 55%',
      secondary: '195 50% 20%',
      accent: '350 85% 60%',
      success: '160 70% 45%',
      warning: '45 100% 55%',
      error: '0 72% 51%',
      info: '180 60% 50%',
      background: '195 60% 12%',
      foreground: '180 30% 95%',
      card: '195 50% 15%',
      border: '195 40% 22%',
      muted: '180 20% 60%',
    },
    light: {
      primary: '25 100% 50%',
      secondary: '180 25% 90%',
      accent: '350 80% 55%',
      success: '160 70% 40%',
      warning: '45 100% 50%',
      error: '0 84% 60%',
      info: '180 60% 45%',
      background: '180 30% 96%',
      foreground: '195 60% 15%',
      card: '0 0% 100%',
      border: '180 20% 85%',
      muted: '195 30% 40%',
    },
  },
}

// Midnight Blue - Cool blue tones
const midnightBlue: Theme = {
  id: 'midnight-blue',
  name: 'Midnight Blue',
  description: 'Deep blue with cyan accents',
  colors: {
    dark: {
      primary: '210 100% 55%',
      secondary: '220 50% 20%',
      accent: '280 80% 60%',
      success: '150 70% 45%',
      warning: '40 100% 55%',
      error: '0 72% 51%',
      info: '190 80% 50%',
      background: '220 60% 10%',
      foreground: '210 30% 95%',
      card: '220 50% 14%',
      border: '220 40% 22%',
      muted: '210 20% 60%',
    },
    light: {
      primary: '210 100% 45%',
      secondary: '210 25% 90%',
      accent: '280 70% 55%',
      success: '150 70% 40%',
      warning: '40 100% 50%',
      error: '0 84% 60%',
      info: '190 80% 45%',
      background: '210 30% 96%',
      foreground: '220 60% 15%',
      card: '0 0% 100%',
      border: '210 20% 85%',
      muted: '220 30% 40%',
    },
  },
}

// Forest Green - Natural green tones
const forestGreen: Theme = {
  id: 'forest-green',
  name: 'Forest Green',
  description: 'Natural greens with earthy accents',
  colors: {
    dark: {
      primary: '140 70% 45%',
      secondary: '150 40% 18%',
      accent: '30 80% 55%',
      success: '120 60% 45%',
      warning: '45 100% 55%',
      error: '0 72% 51%',
      info: '180 50% 50%',
      background: '150 50% 10%',
      foreground: '140 30% 95%',
      card: '150 45% 14%',
      border: '150 35% 22%',
      muted: '140 20% 60%',
    },
    light: {
      primary: '140 70% 38%',
      secondary: '140 25% 90%',
      accent: '30 70% 50%',
      success: '120 60% 40%',
      warning: '45 100% 50%',
      error: '0 84% 60%',
      info: '180 50% 45%',
      background: '140 30% 96%',
      foreground: '150 60% 15%',
      card: '0 0% 100%',
      border: '140 20% 85%',
      muted: '150 30% 40%',
    },
  },
}

// Purple Haze - Vibrant purple theme
const purpleHaze: Theme = {
  id: 'purple-haze',
  name: 'Purple Haze',
  description: 'Vibrant purples with pink accents',
  colors: {
    dark: {
      primary: '270 80% 60%',
      secondary: '280 40% 20%',
      accent: '330 85% 60%',
      success: '160 70% 45%',
      warning: '45 100% 55%',
      error: '0 72% 51%',
      info: '200 70% 55%',
      background: '280 50% 10%',
      foreground: '270 30% 95%',
      card: '280 45% 14%',
      border: '280 35% 22%',
      muted: '270 20% 60%',
    },
    light: {
      primary: '270 80% 50%',
      secondary: '270 25% 90%',
      accent: '330 75% 55%',
      success: '160 70% 40%',
      warning: '45 100% 50%',
      error: '0 84% 60%',
      info: '200 70% 45%',
      background: '270 30% 96%',
      foreground: '280 60% 15%',
      card: '0 0% 100%',
      border: '270 20% 85%',
      muted: '280 30% 40%',
    },
  },
}

// Monochrome - Clean grayscale
const monochrome: Theme = {
  id: 'monochrome',
  name: 'Monochrome',
  description: 'Clean black and white with gray accents',
  colors: {
    dark: {
      primary: '0 0% 90%',
      secondary: '0 0% 18%',
      accent: '0 0% 70%',
      success: '140 50% 50%',
      warning: '45 80% 55%',
      error: '0 65% 55%',
      info: '200 50% 55%',
      background: '0 0% 8%',
      foreground: '0 0% 95%',
      card: '0 0% 12%',
      border: '0 0% 20%',
      muted: '0 0% 55%',
    },
    light: {
      primary: '0 0% 15%',
      secondary: '0 0% 92%',
      accent: '0 0% 40%',
      success: '140 50% 45%',
      warning: '45 80% 50%',
      error: '0 65% 50%',
      info: '200 50% 50%',
      background: '0 0% 98%',
      foreground: '0 0% 10%',
      card: '0 0% 100%',
      border: '0 0% 85%',
      muted: '0 0% 45%',
    },
  },
}

// Forward to the Past - Windows 3.1/95 Retro Theme
const forwardToPast: Theme = {
  id: 'forward-to-past',
  name: 'Forward to the Past',
  description: 'Retro Windows 3.1/95 nostalgia',
  colors: {
    dark: {
      // Dark mode: CRT monitor aesthetic with readable contrast
      primary: '213 100% 50%',         // Bright blue for visibility (#0066FF)
      secondary: '0 0% 20%',           // Dark gray panels
      accent: '180 100% 35%',          // Teal accent (#00B3B3)
      success: '120 100% 35%',         // Bright green (#00B300)
      warning: '45 100% 50%',          // Amber (#FFB300)
      error: '0 85% 55%',              // Bright red (#E53935)
      info: '195 100% 45%',            // Cyan (#00B8E6)
      background: '220 15% 10%',       // Dark blue-gray CRT (#161A1F)
      foreground: '60 10% 90%',        // Warm white text (#E6E4DF)
      card: '220 12% 16%',             // Slightly lighter card (#24282E)
      border: '220 10% 25%',           // Visible border (#383D45)
      muted: '220 10% 55%',            // Readable muted text (#848A94)
    },
    light: {
      // Light mode: Classic Windows 95 desktop
      primary: '240 100% 30%',         // Navy Blue (#000099)
      secondary: '0 0% 85%',           // Light silver (#D9D9D9)
      accent: '180 100% 28%',          // Classic Teal (#008F8F)
      success: '120 100% 28%',         // DOS Green (#008F00)
      warning: '40 100% 40%',          // Dark amber for contrast (#CC8800)
      error: '0 85% 45%',              // Dark red (#D32F2F)
      info: '200 100% 35%',            // Dark cyan (#0077B3)
      background: '180 60% 35%',       // Classic Teal Desktop (#248F8F)
      foreground: '0 0% 0%',           // Pure black text
      card: '0 0% 75%',                // Classic silver (#C0C0C0)
      border: '0 0% 45%',              // Medium gray border (#737373)
      muted: '0 0% 30%',               // Dark gray muted (#4D4D4D)
    },
  },
}

export const themes: Theme[] = [oceanSunset, midnightBlue, forestGreen, purpleHaze, monochrome, forwardToPast]

// ==========================================
// FONT DEFINITIONS
// ==========================================

export type FontId = 'pixellet' | 'plank' | 'super-croissant' | 'spicy-sale' | 'star' | 'playful-christmas' | 'inter' | 'jetbrains' | 'fira-code' | 'roboto-mono' | 'system'

export interface Font {
  id: FontId
  name: string
  description: string
  family: string
  category: 'pixel' | 'display' | 'sans-serif' | 'monospace'
}

export const fonts: Font[] = [
  // Local Fonts
  {
    id: 'pixellet',
    name: 'Pixellet',
    description: 'Retro pixel font',
    family: "'Pixellet', system-ui, sans-serif",
    category: 'pixel',
  },
  {
    id: 'plank',
    name: 'Plank',
    description: 'Bold display font',
    family: "'Plank', system-ui, sans-serif",
    category: 'display',
  },
  {
    id: 'super-croissant',
    name: 'Super Croissant',
    description: 'Playful rounded font',
    family: "'Super Croissant', system-ui, sans-serif",
    category: 'display',
  },
  {
    id: 'spicy-sale',
    name: 'Spicy Sale',
    description: 'Fun decorative font',
    family: "'Spicy Sale', system-ui, sans-serif",
    category: 'display',
  },
  {
    id: 'star',
    name: 'Star',
    description: 'Starry display font',
    family: "'Star', system-ui, sans-serif",
    category: 'display',
  },
  {
    id: 'playful-christmas',
    name: 'Playful Christmas',
    description: 'Festive holiday font',
    family: "'Playful Christmas', system-ui, sans-serif",
    category: 'display',
  },
  // Web Fonts
  {
    id: 'inter',
    name: 'Inter',
    description: 'Modern sans-serif',
    family: "'Inter', system-ui, sans-serif",
    category: 'sans-serif',
  },
  {
    id: 'jetbrains',
    name: 'JetBrains Mono',
    description: 'Developer-focused monospace',
    family: "'JetBrains Mono', 'Fira Code', monospace",
    category: 'monospace',
  },
  {
    id: 'fira-code',
    name: 'Fira Code',
    description: 'Monospace with ligatures',
    family: "'Fira Code', 'JetBrains Mono', monospace",
    category: 'monospace',
  },
  {
    id: 'roboto-mono',
    name: 'Roboto Mono',
    description: 'Clean monospace',
    family: "'Roboto Mono', 'Consolas', monospace",
    category: 'monospace',
  },
  {
    id: 'system',
    name: 'System Default',
    description: 'Use system fonts',
    family: "system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif",
    category: 'sans-serif',
  },
]

// ==========================================
// GLOW INTENSITY DEFINITIONS
// ==========================================

export type GlowIntensity = 'none' | 'subtle' | 'normal' | 'high'

export const glowIntensities: { id: GlowIntensity; name: string; multiplier: number }[] = [
  { id: 'none', name: 'None', multiplier: 0 },
  { id: 'subtle', name: 'Subtle', multiplier: 0.5 },
  { id: 'normal', name: 'Normal', multiplier: 1 },
  { id: 'high', name: 'High', multiplier: 1.5 },
]

// ==========================================
// APPEARANCE CONTEXT
// ==========================================

export interface AppearanceSettings {
  themeId: ThemeId
  fontId: FontId
  colorMode: 'dark' | 'light'
  glowIntensity: GlowIntensity
  animationsEnabled: boolean
  fontSize: 'small' | 'medium' | 'large'
}

const defaultSettings: AppearanceSettings = {
  themeId: 'ocean-sunset',
  fontId: 'pixellet',
  colorMode: 'dark',
  glowIntensity: 'subtle',
  animationsEnabled: true,
  fontSize: 'medium',
}

interface AppearanceContextType {
  settings: AppearanceSettings
  currentTheme: Theme
  currentFont: Font
  
  // Setters
  setTheme: (themeId: ThemeId) => void
  setFont: (fontId: FontId) => void
  setColorMode: (mode: 'dark' | 'light') => void
  setGlowIntensity: (intensity: GlowIntensity) => void
  setAnimationsEnabled: (enabled: boolean) => void
  setFontSize: (size: 'small' | 'medium' | 'large') => void
  
  // Helpers
  toggleColorMode: () => void
  resetToDefaults: () => void
  
  // Theme & Font Lists
  availableThemes: Theme[]
  availableFonts: Font[]
}

const AppearanceContext = createContext<AppearanceContextType | undefined>(undefined)

const STORAGE_KEY = 'sentinelfs-appearance'

export function AppearanceProvider({ children }: { children: ReactNode }) {
  const [settings, setSettings] = useState<AppearanceSettings>(() => {
    try {
      const saved = localStorage.getItem(STORAGE_KEY)
      if (saved) {
        return { ...defaultSettings, ...JSON.parse(saved) }
      }
    } catch {
      console.warn('Failed to load appearance settings')
    }
    return defaultSettings
  })

  const currentTheme = themes.find(t => t.id === settings.themeId) || themes[0]
  const currentFont = fonts.find(f => f.id === settings.fontId) || fonts[0]

  // Persist settings to localStorage
  useEffect(() => {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(settings))
    } catch {
      console.warn('Failed to save appearance settings')
    }
  }, [settings])

  // Apply color mode to document
  useEffect(() => {
    document.documentElement.classList.remove('light', 'dark')
    document.documentElement.classList.add(settings.colorMode)
  }, [settings.colorMode])

  // Apply theme colors as CSS variables
  useEffect(() => {
    const colors = settings.colorMode === 'dark' 
      ? currentTheme.colors.dark 
      : currentTheme.colors.light

    const root = document.documentElement
    Object.entries(colors).forEach(([key, value]) => {
      root.style.setProperty(`--${key}`, value)
    })
    
    // Set data-theme attribute for theme-specific CSS (e.g., retro theme)
    root.setAttribute('data-theme', settings.themeId)
  }, [currentTheme, settings.colorMode, settings.themeId])

  // Apply font family
  useEffect(() => {
    document.documentElement.style.setProperty('--font-family', currentFont.family)
    
    // Update the global font override in index.css via class
    document.documentElement.setAttribute('data-font', settings.fontId)
  }, [currentFont, settings.fontId])

  // Apply glow intensity
  useEffect(() => {
    document.documentElement.setAttribute('data-glow', settings.glowIntensity)
  }, [settings.glowIntensity])

  // Apply animations setting
  useEffect(() => {
    document.documentElement.setAttribute('data-animations', settings.animationsEnabled ? 'true' : 'false')
  }, [settings.animationsEnabled])

  // Apply font size
  useEffect(() => {
    document.documentElement.setAttribute('data-font-size', settings.fontSize)
  }, [settings.fontSize])

  // Setters
  const setTheme = useCallback((themeId: ThemeId) => {
    setSettings(prev => ({ ...prev, themeId }))
  }, [])

  const setFont = useCallback((fontId: FontId) => {
    setSettings(prev => ({ ...prev, fontId }))
  }, [])

  const setColorMode = useCallback((colorMode: 'dark' | 'light') => {
    setSettings(prev => ({ ...prev, colorMode }))
  }, [])

  const setGlowIntensity = useCallback((glowIntensity: GlowIntensity) => {
    setSettings(prev => ({ ...prev, glowIntensity }))
  }, [])

  const setAnimationsEnabled = useCallback((animationsEnabled: boolean) => {
    setSettings(prev => ({ ...prev, animationsEnabled }))
  }, [])

  const setFontSize = useCallback((fontSize: 'small' | 'medium' | 'large') => {
    setSettings(prev => ({ ...prev, fontSize }))
  }, [])

  const toggleColorMode = useCallback(() => {
    setSettings(prev => ({ 
      ...prev, 
      colorMode: prev.colorMode === 'dark' ? 'light' : 'dark' 
    }))
  }, [])

  const resetToDefaults = useCallback(() => {
    setSettings(defaultSettings)
  }, [])

  return (
    <AppearanceContext.Provider 
      value={{ 
        settings,
        currentTheme,
        currentFont,
        setTheme,
        setFont,
        setColorMode,
        setGlowIntensity,
        setAnimationsEnabled,
        setFontSize,
        toggleColorMode,
        resetToDefaults,
        availableThemes: themes,
        availableFonts: fonts,
      }}
    >
      {children}
    </AppearanceContext.Provider>
  )
}

export function useAppearance() {
  const context = useContext(AppearanceContext)
  if (!context) {
    throw new Error('useAppearance must be used within an AppearanceProvider')
  }
  return context
}
