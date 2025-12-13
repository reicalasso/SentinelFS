import { useState, useEffect } from 'react'
import { Database, Trash2, RotateCcw } from 'lucide-react'
import { useNotifications } from '../../context/NotificationContext'

interface FalconStoreSettingsProps {
  onLog?: (message: string) => void
}

export function FalconStoreSettings({ onLog }: FalconStoreSettingsProps) {
  const { addNotification } = useNotifications()
  
  // Database settings state
  const [cacheSize, setCacheSize] = useState(1000)
  const [walMode, setWalMode] = useState(true)
  const [stats, setStats] = useState({
    size: '0 KB',
    version: 'v7',
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
              setStats(data.payload)
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
      {/* Database Status */}
      <div className="space-y-4">
        <h3 className="text-lg font-semibold flex items-center gap-2">
          <Database className="w-5 h-5" />
          Database Status
        </h3>
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <div className="p-4 rounded-lg bg-secondary/20">
            <p className="text-xs text-muted-foreground">Size</p>
            <p className="text-lg font-semibold">{stats.size}</p>
          </div>
          <div className="p-4 rounded-lg bg-secondary/20">
            <p className="text-xs text-muted-foreground">Version</p>
            <p className="text-lg font-semibold">{stats.version}</p>
          </div>
          <div className="p-4 rounded-lg bg-secondary/20">
            <p className="text-xs text-muted-foreground">Tables</p>
            <p className="text-lg font-semibold">{stats.tables}</p>
          </div>
          <div className="p-4 rounded-lg bg-secondary/20">
            <p className="text-xs text-muted-foreground">Last Optimized</p>
            <p className="text-lg font-semibold">{stats.lastOptimized}</p>
          </div>
        </div>
      </div>
      
      {/* Query Cache Settings */}
      <div className="space-y-4">
        <h3 className="text-lg font-semibold">Query Cache</h3>
        <div className="space-y-2">
          <label className="text-sm">Cache Size (entries)</label>
          <input
            type="number"
            value={cacheSize}
            onChange={(e) => setCacheSize(Number(e.target.value))}
            className="w-full p-2 rounded-lg bg-background border border-border/40"
          />
          <p className="text-xs text-muted-foreground">
            Higher values improve query performance but use more memory
          </p>
        </div>
      </div>
      
      {/* Performance Settings */}
      <div className="space-y-4">
        <h3 className="text-lg font-semibold">Performance</h3>
        <div className="flex items-center justify-between">
          <div>
            <label className="text-sm">WAL Mode</label>
            <p className="text-xs text-muted-foreground">Better concurrency for read/write operations</p>
          </div>
          <button
            onClick={() => setWalMode(!walMode)}
            className={`w-12 h-6 rounded-full transition-colors ${
              walMode ? 'bg-primary' : 'bg-secondary'
            }`}
          >
            <div className={`w-5 h-5 bg-background rounded-full transition-transform ${
              walMode ? 'translate-x-6' : 'translate-x-0.5'
            }`} />
          </button>
        </div>
      </div>
      
      {/* Maintenance Actions */}
      <div className="space-y-4">
        <h3 className="text-lg font-semibold">Maintenance</h3>
        <div className="flex flex-wrap gap-3">
          <button
            onClick={handleVacuum}
            className="flex items-center gap-2 px-4 py-2 rounded-lg bg-secondary hover:bg-secondary/80 text-sm font-medium"
          >
            <Trash2 className="w-4 h-4" />
            Vacuum Database
          </button>
          <button
            onClick={handleClearCache}
            className="flex items-center gap-2 px-4 py-2 rounded-lg bg-secondary hover:bg-secondary/80 text-sm font-medium"
          >
            <RotateCcw className="w-4 h-4" />
            Clear Cache
          </button>
        </div>
        <p className="text-xs text-muted-foreground">
          Vacuum optimizes database size and performance. Clear cache removes stored query results.
        </p>
      </div>
      
      {/* Save Button */}
      <div className="flex justify-end">
        <button
          onClick={handleSaveSettings}
          className="px-6 py-2 bg-primary text-primary-foreground rounded-lg hover:bg-primary/90 font-medium"
        >
          Save Settings
        </button>
      </div>
    </div>
  )
}
