import { useEffect, useState } from 'react'
import { Database, RefreshCw, Zap, Download, HardDrive, Clock, Activity, BarChart3, Shield, Layers } from 'lucide-react'

interface FalconStoreStatus {
  plugin: string
  version: string
  initialized: boolean
  schemaVersion: number
  latestVersion: number
  status: string
  cache: {
    enabled: boolean
    entries: number
    hits: number
    misses: number
    hitRate: number
    memoryUsed: number
  }
}

interface FalconStoreStats {
  totalQueries: number
  selectQueries: number
  insertQueries: number
  updateQueries: number
  deleteQueries: number
  avgQueryTimeMs: number
  maxQueryTimeMs: number
  slowQueries: number
  dbSizeBytes: number
  schemaVersion: number
  cache: {
    hits: number
    misses: number
    entries: number
    memoryUsed: number
    hitRate: number
  }
}

const formatBytes = (bytes: number): string => {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

export const FalconStore: React.FC = () => {
  const [status, setStatus] = useState<FalconStoreStatus | null>(null)
  const [stats, setStats] = useState<FalconStoreStats | null>(null)
  const [loading, setLoading] = useState(true)
  const [optimizing, setOptimizing] = useState(false)
  const [backingUp, setBackingUp] = useState(false)

  useEffect(() => {
    if (!window.api) return

    const handleDaemonData = (data: { type: string; payload: any }) => {
      if (data.type === 'FALCONSTORE_STATUS' && data.payload) {
        setStatus({...data.payload})
        setLoading(false)
      } else if (data.type === 'FALCONSTORE_STATS' && data.payload) {
        setStats({...data.payload})
        setLoading(false)
      }
    }

    window.api.on('daemon-data', handleDaemonData)
    
    // Initial request
    window.api.sendCommand('FALCONSTORE_STATUS')
    window.api.sendCommand('FALCONSTORE_STATS')

    return () => {
      window.api.off('daemon-data', handleDaemonData)
    }
  }, [])

  const handleRefresh = () => {
    window.api?.sendCommand('FALCONSTORE_STATUS')
    window.api?.sendCommand('FALCONSTORE_STATS')
  }

  const handleOptimize = async () => {
    setOptimizing(true)
    try {
      await window.api?.sendCommand('FALCONSTORE_OPTIMIZE')
      setTimeout(() => {
        handleRefresh()
        setOptimizing(false)
      }, 1000)
    } catch {
      setOptimizing(false)
    }
  }

  const handleBackup = async () => {
    setBackingUp(true)
    try {
      await window.api?.sendCommand('FALCONSTORE_BACKUP')
      setTimeout(() => {
        setBackingUp(false)
      }, 2000)
    } catch {
      setBackingUp(false)
    }
  }

  if (loading) {
    return (
      <div className="flex items-center justify-center h-64">
        <div className="flex items-center gap-3 text-muted-foreground">
          <RefreshCw className="w-5 h-5 animate-spin" />
          <span>Loading FalconStore data...</span>
        </div>
      </div>
    )
  }

  const queryTotal = stats?.totalQueries || 1
  const selectPercent = ((stats?.selectQueries || 0) / queryTotal) * 100
  const insertPercent = ((stats?.insertQueries || 0) / queryTotal) * 100
  const updatePercent = ((stats?.updateQueries || 0) / queryTotal) * 100
  const deletePercent = ((stats?.deleteQueries || 0) / queryTotal) * 100

  return (
    <div className="space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
      {/* Header */}
      <div className="flex flex-col sm:flex-row sm:items-center sm:justify-between gap-4">
        <div className="flex items-center gap-4">
          <div className="p-3 rounded-2xl bg-gradient-to-br from-orange-500/15 to-orange-500/5 border border-orange-500/20">
            <Database className="w-7 h-7 text-orange-500" />
          </div>
          <div>
            <h2 className="text-2xl font-bold flex items-center gap-3">
              FalconStore
              <span className="text-sm font-normal text-muted-foreground">v{status?.version || '1.0.0'}</span>
              <span className={`px-2 py-0.5 rounded-lg text-xs font-medium ${
                status?.status === 'running' 
                  ? 'bg-success/10 text-success border border-success/20' 
                  : 'bg-error/10 text-error border border-error/20'
              }`}>
                {status?.status === 'running' ? '● Running' : '○ Stopped'}
              </span>
            </h2>
            <p className="text-sm text-muted-foreground">High-performance storage engine with migration system</p>
          </div>
        </div>
        <button 
          onClick={handleRefresh}
          className="flex items-center gap-2 px-4 py-2 rounded-xl bg-secondary/60 hover:bg-secondary text-sm font-medium transition-all border border-border/40"
        >
          <RefreshCw className="w-4 h-4" />
          Refresh
        </button>
      </div>

      {/* Stats Grid */}
      <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
        <StatCard
          icon={<HardDrive className="w-5 h-5" />}
          label="Database Size"
          value={formatBytes(stats?.dbSizeBytes || 0)}
          color="blue"
        />
        <StatCard
          icon={<Activity className="w-5 h-5" />}
          label="Total Queries"
          value={(stats?.totalQueries || 0).toLocaleString()}
          color="green"
        />
        <StatCard
          icon={<Clock className="w-5 h-5" />}
          label="Avg Query Time"
          value={`${(stats?.avgQueryTimeMs || 0).toFixed(2)}ms`}
          color="purple"
        />
        <StatCard
          icon={<Layers className="w-5 h-5" />}
          label="Schema Version"
          value={`v${stats?.schemaVersion || 0}`}
          color="orange"
        />
      </div>

      {/* Main Content Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {/* Query Distribution */}
        <div className="rounded-2xl border border-border/40 bg-card/50 p-6">
          <div className="flex items-center gap-3 mb-6">
            <BarChart3 className="w-5 h-5 text-primary" />
            <h3 className="font-semibold">Query Distribution</h3>
          </div>
          
          <div className="space-y-4">
            <QueryBar 
              label="SELECT" 
              count={stats?.selectQueries || 0} 
              percent={selectPercent} 
              color="bg-blue-500" 
            />
            <QueryBar 
              label="INSERT" 
              count={stats?.insertQueries || 0} 
              percent={insertPercent} 
              color="bg-green-500" 
            />
            <QueryBar 
              label="UPDATE" 
              count={stats?.updateQueries || 0} 
              percent={updatePercent} 
              color="bg-yellow-500" 
            />
            <QueryBar 
              label="DELETE" 
              count={stats?.deleteQueries || 0} 
              percent={deletePercent} 
              color="bg-red-500" 
            />
          </div>
        </div>

        {/* Cache Performance */}
        <div className="rounded-2xl border border-border/40 bg-card/50 p-6">
          <div className="flex items-center gap-3 mb-6">
            <Shield className="w-5 h-5 text-primary" />
            <h3 className="font-semibold">Cache Performance</h3>
          </div>
          
          {/* Hit Rate Circle */}
          <div className="flex items-center justify-center mb-6">
            <div className="relative w-32 h-32">
              <svg className="w-full h-full transform -rotate-90">
                <circle
                  cx="64"
                  cy="64"
                  r="56"
                  stroke="currentColor"
                  strokeWidth="12"
                  fill="none"
                  className="text-secondary"
                />
                <circle
                  cx="64"
                  cy="64"
                  r="56"
                  stroke="currentColor"
                  strokeWidth="12"
                  fill="none"
                  strokeDasharray={`${(stats?.cache?.hitRate || 0) * 3.52} 352`}
                  className="text-primary transition-all duration-500"
                />
              </svg>
              <div className="absolute inset-0 flex flex-col items-center justify-center">
                <span className="text-2xl font-bold">{(stats?.cache?.hitRate || 0).toFixed(1)}%</span>
                <span className="text-xs text-muted-foreground">Hit Rate</span>
              </div>
            </div>
          </div>

          {/* Cache Stats */}
          <div className="grid grid-cols-2 gap-4">
            <div className="text-center p-3 rounded-xl bg-secondary/30">
              <div className="text-lg font-semibold text-success">{stats?.cache?.hits || 0}</div>
              <div className="text-xs text-muted-foreground">Cache Hits</div>
            </div>
            <div className="text-center p-3 rounded-xl bg-secondary/30">
              <div className="text-lg font-semibold text-error">{stats?.cache?.misses || 0}</div>
              <div className="text-xs text-muted-foreground">Cache Misses</div>
            </div>
            <div className="text-center p-3 rounded-xl bg-secondary/30">
              <div className="text-lg font-semibold">{stats?.cache?.entries || 0}</div>
              <div className="text-xs text-muted-foreground">Entries</div>
            </div>
            <div className="text-center p-3 rounded-xl bg-secondary/30">
              <div className="text-lg font-semibold">{formatBytes(stats?.cache?.memoryUsed || 0)}</div>
              <div className="text-xs text-muted-foreground">Memory Used</div>
            </div>
          </div>
        </div>
      </div>

      {/* Performance Metrics */}
      <div className="rounded-2xl border border-border/40 bg-card/50 p-6">
        <div className="flex items-center gap-3 mb-6">
          <Activity className="w-5 h-5 text-primary" />
          <h3 className="font-semibold">Performance Metrics</h3>
        </div>
        
        <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
          <div className="p-4 rounded-xl bg-secondary/30 border border-border/30">
            <div className="text-2xl font-bold">{(stats?.avgQueryTimeMs || 0).toFixed(2)}ms</div>
            <div className="text-sm text-muted-foreground">Average Query Time</div>
          </div>
          <div className="p-4 rounded-xl bg-secondary/30 border border-border/30">
            <div className="text-2xl font-bold">{(stats?.maxQueryTimeMs || 0).toFixed(2)}ms</div>
            <div className="text-sm text-muted-foreground">Max Query Time</div>
          </div>
          <div className="p-4 rounded-xl bg-secondary/30 border border-border/30">
            <div className={`text-2xl font-bold ${(stats?.slowQueries || 0) > 0 ? 'text-warning' : 'text-success'}`}>
              {stats?.slowQueries || 0}
            </div>
            <div className="text-sm text-muted-foreground">Slow Queries (&gt;100ms)</div>
          </div>
          <div className="p-4 rounded-xl bg-secondary/30 border border-border/30">
            <div className="text-2xl font-bold">{stats?.schemaVersion || 0}</div>
            <div className="text-sm text-muted-foreground">Migration Version</div>
          </div>
        </div>
      </div>

      {/* Actions */}
      <div className="flex flex-wrap gap-3">
        <button 
          onClick={handleOptimize}
          disabled={optimizing}
          className="flex items-center gap-2 px-5 py-2.5 rounded-xl bg-gradient-to-r from-primary to-primary/80 text-primary-foreground font-medium transition-all hover:shadow-lg hover:shadow-primary/20 disabled:opacity-50"
        >
          <Zap className={`w-4 h-4 ${optimizing ? 'animate-pulse' : ''}`} />
          {optimizing ? 'Optimizing...' : 'Optimize Database'}
        </button>
        <button 
          onClick={handleBackup}
          disabled={backingUp}
          className="flex items-center gap-2 px-5 py-2.5 rounded-xl bg-secondary hover:bg-secondary/80 font-medium transition-all border border-border/40"
        >
          <Download className={`w-4 h-4 ${backingUp ? 'animate-bounce' : ''}`} />
          {backingUp ? 'Creating Backup...' : 'Backup Database'}
        </button>
      </div>
    </div>
  )
}

function StatCard({ icon, label, value, color }: { icon: React.ReactNode; label: string; value: string; color: string }) {
  const colorClasses: Record<string, string> = {
    blue: 'from-blue-500/15 to-blue-500/5 border-blue-500/20 text-blue-500',
    green: 'from-green-500/15 to-green-500/5 border-green-500/20 text-green-500',
    purple: 'from-purple-500/15 to-purple-500/5 border-purple-500/20 text-purple-500',
    orange: 'from-orange-500/15 to-orange-500/5 border-orange-500/20 text-orange-500',
  }

  return (
    <div className={`p-4 rounded-xl bg-gradient-to-br ${colorClasses[color]} border`}>
      <div className="flex items-center gap-2 mb-2">
        {icon}
        <span className="text-xs text-muted-foreground">{label}</span>
      </div>
      <div className="text-xl font-bold text-foreground">{value}</div>
    </div>
  )
}

function QueryBar({ label, count, percent, color }: { label: string; count: number; percent: number; color: string }) {
  return (
    <div className="flex items-center gap-3">
      <span className="w-16 text-sm text-muted-foreground">{label}</span>
      <div className="flex-1 h-3 bg-secondary/50 rounded-full overflow-hidden">
        <div 
          className={`h-full ${color} rounded-full transition-all duration-500`}
          style={{ width: `${Math.max(percent, 0)}%` }}
        />
      </div>
      <span className="w-16 text-sm text-right font-medium">{count.toLocaleString()}</span>
    </div>
  )
}
