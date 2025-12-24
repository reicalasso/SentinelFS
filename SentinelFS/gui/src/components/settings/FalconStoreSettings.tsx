import { useState, useEffect } from 'react'
import { Database, Trash2, RotateCcw, HardDrive, Gauge, Wrench } from 'lucide-react'
import { useNotifications } from '../../context/NotificationContext'
import { Section } from './Section'
import { Toggle } from './Toggle'

interface FalconStoreSettingsProps {
  onLog?: (message: string) => void
}

const formatBytes = (bytes: number): string => {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

export function FalconStoreSettings({ onLog }: FalconStoreSettingsProps) {
  const { addNotification } = useNotifications()
  
  // Database settings state
  const [cacheSize, setCacheSize] = useState(1000)
  const [walMode, setWalMode] = useState(true)
  const [stats, setStats] = useState({
    size: '0 KB',
    version: 'v1',
    tables: 0,
    lastOptimized: 'Never'
  })
  
  // Load current settings
  useEffect(() => {
    const loadSettings = async () => {
      if (window.api) {
        try {
          const res = await window.api.sendCommand('FALCONSTORE_STATUS')
          if (res.success) {
            const data = typeof res === 'string' ? JSON.parse(res) : res
            if (data.payload) {
              setStats({
                size: formatBytes(data.payload.dbSize || 0),
                version: `v${data.payload.schemaVersion || 1}`,
                tables: data.payload.tableCount || 0,
                lastOptimized: 'Never'
              })
            }
          }
        } catch (error) {
          onLog?.('[FalconStore] Failed to load settings: ' + error)
        }
      }
    }
    loadSettings()
  }, [onLog])
  
  const handleVacuum = async () => {
    if (window.api) {
      try {
        await window.api.sendCommand('FALCONSTORE_VACUUM')
        addNotification('success', 'Database Optimized', 'VACUUM operation completed successfully')
        onLog?.('[FalconStore] Database VACUUM completed')
        // Refresh stats
        setTimeout(() => {
          const loadSettings = async () => {
            if (window.api) {
              const res = await window.api.sendCommand('FALCONSTORE_STATUS')
              if (res.success) {
                const data = typeof res === 'string' ? JSON.parse(res) : res
                if (data.payload) {
                  setStats(data.payload)
                }
              }
            }
          }
          loadSettings()
        }, 1000)
      } catch (error) {
        addNotification('error', 'Optimization Failed', 'Failed to optimize database')
        onLog?.('[FalconStore] VACUUM failed: ' + error)
      }
    }
  }
  
  const handleClearCache = async () => {
    if (window.api) {
      try {
        await window.api.sendCommand('FALCONSTORE_CLEAR_CACHE')
        addNotification('success', 'Cache Cleared', 'Query cache has been cleared')
        onLog?.('[FalconStore] Cache cleared')
      } catch (error) {
        addNotification('error', 'Cache Clear Failed', 'Failed to clear cache')
        onLog?.('[FalconStore] Cache clear failed: ' + error)
      }
    }
  }
  
  const handleSaveSettings = async () => {
    if (window.api) {
      try {
        // Save cache size
        await window.api.sendCommand(`FALCONSTORE_SET_CACHE_SIZE ${cacheSize}`)
        // Save WAL mode
        await window.api.sendCommand(`FALCONSTORE_SET_WAL_MODE ${walMode ? '1' : '0'}`)
        
        addNotification('success', 'Settings Saved', 'FalconStore settings have been updated')
        onLog?.('[FalconStore] Settings saved')
      } catch (error) {
        addNotification('error', 'Save Failed', 'Failed to save settings')
        onLog?.('[FalconStore] Failed to save settings: ' + error)
      }
    }
  }
  
  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center gap-3 mb-2">
        <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-emerald-500 to-teal-600 flex items-center justify-center shadow-lg">
          <Database className="w-5 h-5 text-white" />
        </div>
        <div>
          <h3 className="font-bold text-lg flex items-center gap-2">
            FalconStore
            <span className="text-xs px-2 py-0.5 rounded-full bg-emerald-500/20 text-emerald-400 font-medium">{stats.version}</span>
          </h3>
          <p className="text-xs text-muted-foreground">High-performance SQLite storage engine</p>
        </div>
      </div>

      {/* Database Status */}
      <Section title="Database Status">
        <div className="grid grid-cols-2 md:grid-cols-4 gap-3">
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
            <div className="flex items-center gap-2 text-muted-foreground mb-1">
              <HardDrive className="w-4 h-4" />
              <span className="text-xs">Size</span>
            </div>
            <div className="font-mono font-bold">{stats.size}</div>
          </div>
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
            <div className="flex items-center gap-2 text-muted-foreground mb-1">
              <Database className="w-4 h-4" />
              <span className="text-xs">Tables</span>
            </div>
            <div className="font-mono font-bold">{stats.tables}</div>
          </div>
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
            <div className="flex items-center gap-2 text-muted-foreground mb-1">
              <Gauge className="w-4 h-4" />
              <span className="text-xs">Version</span>
            </div>
            <div className="font-mono font-bold">{stats.version}</div>
          </div>
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
            <div className="flex items-center gap-2 text-muted-foreground mb-1">
              <Wrench className="w-4 h-4" />
              <span className="text-xs">Optimized</span>
            </div>
            <div className="font-mono font-bold text-sm">{stats.lastOptimized}</div>
          </div>
        </div>
      </Section>
      
      {/* Query Cache Settings */}
      <Section title="Query Cache">
        <div className="space-y-3">
          <div>
            <label className="settings-label">Cache Size (entries)</label>
            <input
              type="number"
              value={cacheSize}
              onChange={(e) => setCacheSize(Number(e.target.value))}
              className="settings-input"
            />
          </div>
          <p className="text-xs text-muted-foreground">
            Higher values improve query performance but use more memory
          </p>
        </div>
      </Section>
      
      {/* Performance Settings */}
      <Section title="Performance">
        <div className="flex items-center justify-between">
          <div>
            <span className="text-sm font-medium block">WAL Mode</span>
            <span className="text-xs text-muted-foreground">Better concurrency for read/write operations</span>
          </div>
          <Toggle checked={walMode} onChange={() => setWalMode(!walMode)} />
        </div>
      </Section>
      
      {/* Maintenance Actions */}
      <Section title="Maintenance">
        <div className="grid grid-cols-2 gap-3">
          <button
            onClick={handleVacuum}
            className="settings-btn settings-btn-secondary py-3"
          >
            <Trash2 className="w-4 h-4" />
            Vacuum Database
          </button>
          <button
            onClick={handleClearCache}
            className="settings-btn settings-btn-secondary py-3"
          >
            <RotateCcw className="w-4 h-4" />
            Clear Cache
          </button>
        </div>
        <p className="text-xs text-muted-foreground mt-3">
          Vacuum optimizes database size and performance. Clear cache removes stored query results.
        </p>
        
        {/* Save Button */}
        <button
          onClick={handleSaveSettings}
          className="w-full settings-btn settings-btn-primary py-3 mt-4"
        >
          Save Settings
        </button>
      </Section>
    </div>
  )
}
