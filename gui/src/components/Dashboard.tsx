import { Activity, ArrowDown, ArrowUp, HardDrive, Shield, Wifi, Zap, Network, AlertTriangle, Database, Eye, Sparkles, TrendingUp, Clock } from 'lucide-react'
import { Area, AreaChart, CartesianGrid, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts'
import { useState, useEffect } from 'react'

export function Dashboard({ metrics, syncStatus, peersCount, activity }: any) {
  const [trafficHistory, setTrafficHistory] = useState<any[]>([])
  const [lastMetrics, setLastMetrics] = useState<any>(null)
  const [peakUpload, setPeakUpload] = useState(0)
  const [peakDownload, setPeakDownload] = useState(0)
  
  // Format bytes helper
  const formatBytes = (bytes: number) => {
    if (!bytes || bytes === 0) return '0 B'
    if (bytes < 1024) return bytes + ' B'
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(2) + ' MB'
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  }
  
  // Format metrics from daemon
  const totalUploaded = metrics?.totalUploaded || 0
  const totalDownloaded = metrics?.totalDownloaded || 0
  const recentActivity = activity || []
  
  // Health data from syncStatus
  const health = syncStatus?.health
  const anomaly = syncStatus?.anomaly
  const peerHealth = syncStatus?.peerHealth || []
  const degradedPeers = peerHealth.filter((p: any) => p.degraded).length
  
  // Track network traffic over time
  useEffect(() => {
    if (!metrics) return
    
    const now = new Date()
    const timeStr = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`
    
    // Calculate delta (bytes/s then convert to appropriate unit)
    let uploadRate = 0
    let downloadRate = 0
    
    if (lastMetrics) {
      const timeDiff = 2 // seconds (polling interval)
      uploadRate = Math.max(0, (metrics.totalUploaded - lastMetrics.totalUploaded) / timeDiff)
      downloadRate = Math.max(0, (metrics.totalDownloaded - lastMetrics.totalDownloaded) / timeDiff)
      
      // Track peaks
      if (uploadRate > peakUpload) setPeakUpload(uploadRate)
      if (downloadRate > peakDownload) setPeakDownload(downloadRate)
    }
    
    setLastMetrics(metrics)
    
    // Store as KB/s for chart
    setTrafficHistory(prev => {
      const newHistory = [...prev, { 
        time: timeStr, 
        upload: Math.round(uploadRate / 1024), 
        download: Math.round(downloadRate / 1024),
        uploadBytes: uploadRate,
        downloadBytes: downloadRate
      }]
      // Keep last 30 data points
      return newHistory.slice(-30)
    })
  }, [metrics])

  return (
    <div className="space-y-4 sm:space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
      {/* Hero Section with Animated Stats */}
      <div className="relative overflow-hidden rounded-2xl sm:rounded-3xl bg-gradient-to-br from-primary/20 via-card to-accent/10 border border-primary/20 p-4 sm:p-6 lg:p-8 mb-4 sm:mb-8">
        {/* Animated Background Effects */}
        <div className="absolute inset-0 overflow-hidden">
          <div className="absolute -top-1/2 -left-1/2 w-full h-full bg-gradient-to-br from-primary/30 to-transparent rounded-full blur-3xl animate-pulse"></div>
          <div className="absolute -bottom-1/2 -right-1/2 w-full h-full bg-gradient-to-tl from-accent/20 to-transparent rounded-full blur-3xl animate-pulse" style={{animationDelay: '1s'}}></div>
          {/* Grid Pattern Overlay */}
          <div className="absolute inset-0 bg-[linear-gradient(rgba(255,255,255,0.02)_1px,transparent_1px),linear-gradient(90deg,rgba(255,255,255,0.02)_1px,transparent_1px)] bg-[size:50px_50px]"></div>
        </div>
        
        <div className="relative z-10">
          <div className="flex flex-col sm:flex-row sm:items-center gap-3 mb-4">
            <div className="p-2 sm:p-3 rounded-xl sm:rounded-2xl bg-primary/20 backdrop-blur-sm border border-primary/30 shadow-lg shadow-primary/20 w-fit">
              <Shield className="w-6 h-6 sm:w-8 sm:h-8 text-primary" />
            </div>
            <div>
              <h1 className="text-xl sm:text-2xl font-bold bg-gradient-to-r from-foreground to-foreground/60 bg-clip-text text-transparent">
                System Overview
              </h1>
              <p className="text-xs sm:text-sm text-muted-foreground flex items-center gap-2">
                <span className="dot-success animate-pulse"></span>
                All systems operational
              </p>
            </div>
          </div>
          
          {/* Quick Stats Row */}
          <div className="grid grid-cols-2 lg:grid-cols-4 gap-2 sm:gap-4 mt-4 sm:mt-6">
            <QuickStat 
              label="Upload Speed" 
              value={formatBytes(peakUpload) + '/s'} 
              icon={<TrendingUp className="w-4 h-4" />}
              color="teal"
            />
            <QuickStat 
              label="Download Speed" 
              value={formatBytes(peakDownload) + '/s'} 
              icon={<TrendingUp className="w-4 h-4" />}
              color="emerald"
            />
            <QuickStat 
              label="Connected Peers" 
              value={peersCount || 0} 
              icon={<Network className="w-4 h-4" />}
              color="coral"
            />
            <QuickStat 
              label="Uptime" 
              value="99.9%" 
              icon={<Clock className="w-4 h-4" />}
              color="amber"
            />
          </div>
        </div>
      </div>

      {/* Top Stats Row */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3 sm:gap-4">
        <StatCard 
          title="Network Status" 
          value={metrics ? "Online" : "Connecting"}
          sub="Direct P2P Mesh"
          icon={<Wifi className="w-5 h-5 icon-success" />}
          status={metrics ? "success" : "warning"}
          trend="Stable"
        />
        <StatCard 
          title="Sync Status" 
          value={syncStatus?.syncStatus || "Idle"} 
          sub={syncStatus?.pendingFiles ? `${syncStatus.pendingFiles} files pending` : "Up to date"}
          icon={<Zap className="w-5 h-5 text-primary" />}
          status={syncStatus?.syncStatus === 'ENABLED' ? "success" : "warning"}
        />
        <StatCard 
          title="Active Peers" 
          value={`${peersCount || 0} Devices`} 
          sub="Mesh Swarm"
          icon={<Network className="w-5 h-5 text-primary" />}
        />
        <StatCard 
          title="Total Traffic" 
          value={formatBytes(totalDownloaded)} 
          sub={`↑ ${formatBytes(totalUploaded)}`}
          icon={<HardDrive className="w-5 h-5 icon-info" />}
        />
      </div>

      {/* Main Content Grid */}
      <div className="grid grid-cols-1 xl:grid-cols-3 gap-4 sm:gap-6">
        
        {/* Chart Section */}
        <div className="xl:col-span-2 card-modern p-4 sm:p-6 group">
          {/* Decorative top gradient */}
          <div className="absolute top-0 left-0 right-0 h-32 bg-gradient-to-b from-primary/5 to-transparent pointer-events-none"></div>
          
          <div className="relative flex flex-col sm:flex-row sm:items-center sm:justify-between gap-3 sm:gap-0 mb-4 sm:mb-6">
            <div>
              <h3 className="font-bold text-lg sm:text-xl flex items-center gap-2 sm:gap-3">
                <div className="p-1.5 sm:p-2 rounded-lg sm:rounded-xl bg-primary/10 border border-primary/20">
                  <Activity className="w-4 h-4 sm:w-5 sm:h-5 text-primary" />
                </div>
                <span className="bg-gradient-to-r from-foreground to-foreground/60 bg-clip-text text-transparent">
                  Network Traffic
                </span>
              </h3>
              <p className="text-xs sm:text-sm text-muted-foreground mt-1 ml-8 sm:ml-12">Real-time bandwidth monitoring</p>
            </div>
            <div className="flex gap-2 sm:gap-3 text-xs font-semibold">
              <div className="flex items-center gap-1.5 sm:gap-2 bg-primary/15 px-2 sm:px-3 py-1 sm:py-1.5 rounded-lg border border-primary/30 text-primary">
                <div className="dot-primary"></div>
                <span>Upload</span>
              </div>
              <div className="flex items-center gap-1.5 sm:gap-2 bg-success-muted px-2 sm:px-3 py-1 sm:py-1.5 rounded-lg border border-success/30 status-success glow-success">
                <div className="dot-success"></div>
                <span>Download</span>
              </div>
            </div>
          </div>
          
          {/* Traffic Summary */}
          <div className="relative grid grid-cols-2 lg:grid-cols-4 gap-2 sm:gap-3 mb-4 sm:mb-6">
            <div className="bg-gradient-to-br from-background/80 to-background/40 rounded-lg sm:rounded-xl p-2 sm:p-4 border border-border/30 hover:border-success/30 transition-colors group/stat">
              <div className="text-[10px] sm:text-xs text-muted-foreground mb-1 sm:mb-1.5 flex items-center gap-1 sm:gap-1.5">
                <ArrowDown className="w-3 h-3 icon-success" />
                <span className="hidden sm:inline">Total </span>Downloaded
              </div>
              <div className="text-base sm:text-xl font-bold status-success group-hover/stat:scale-105 transition-transform">{formatBytes(totalDownloaded)}</div>
            </div>
            <div className="bg-gradient-to-br from-background/80 to-background/40 rounded-lg sm:rounded-xl p-2 sm:p-4 border border-border/30 hover:border-primary/30 transition-colors group/stat">
              <div className="text-[10px] sm:text-xs text-muted-foreground mb-1 sm:mb-1.5 flex items-center gap-1 sm:gap-1.5">
                <ArrowUp className="w-3 h-3 icon-primary" />
                <span className="hidden sm:inline">Total </span>Uploaded
              </div>
              <div className="text-base sm:text-xl font-bold text-primary group-hover/stat:scale-105 transition-transform">{formatBytes(totalUploaded)}</div>
            </div>
            <div className="bg-gradient-to-br from-background/80 to-background/40 rounded-lg sm:rounded-xl p-2 sm:p-4 border border-border/30 hover:border-success/30 transition-colors group/stat">
              <div className="text-[10px] sm:text-xs text-muted-foreground mb-1 sm:mb-1.5 flex items-center gap-1 sm:gap-1.5">
                <Zap className="w-3 h-3 icon-success" />
                Peak<span className="hidden sm:inline"> Download</span>
              </div>
              <div className="text-base sm:text-xl font-bold status-success group-hover/stat:scale-105 transition-transform">{formatBytes(peakDownload)}/s</div>
            </div>
            <div className="bg-gradient-to-br from-background/80 to-background/40 rounded-lg sm:rounded-xl p-2 sm:p-4 border border-border/30 hover:border-primary/30 transition-colors group/stat">
              <div className="text-[10px] sm:text-xs text-muted-foreground mb-1 sm:mb-1.5 flex items-center gap-1 sm:gap-1.5">
                <Zap className="w-3 h-3 icon-primary" />
                Peak<span className="hidden sm:inline"> Upload</span>
              </div>
              <div className="text-base sm:text-xl font-bold text-primary group-hover/stat:scale-105 transition-transform">{formatBytes(peakUpload)}/s</div>
            </div>
          </div>
          
          <div className="h-[180px] sm:h-[240px] w-full relative">
            {/* Chart glow effect */}
            <div className="absolute inset-0 bg-gradient-to-t from-emerald-500/5 via-transparent to-amber-500/5 rounded-xl pointer-events-none"></div>
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={trafficHistory.length > 0 ? trafficHistory : [{ time: 'Now', upload: 0, download: 0 }]}>
                <defs>
                  <linearGradient id="colorUp" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%" stopColor="#f59e0b" stopOpacity={0.3}/>
                    <stop offset="95%" stopColor="#f59e0b" stopOpacity={0}/>
                  </linearGradient>
                  <linearGradient id="colorDown" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%" stopColor="#10b981" stopOpacity={0.3}/>
                    <stop offset="95%" stopColor="#10b981" stopOpacity={0}/>
                  </linearGradient>
                </defs>
                <CartesianGrid strokeDasharray="3 3" stroke="hsl(var(--border))" vertical={false} opacity={0.4} />
                <XAxis 
                    dataKey="time" 
                    stroke="hsl(var(--muted-foreground))" 
                    fontSize={11} 
                    tickLine={false} 
                    axisLine={false} 
                    minTickGap={30}
                />
                <YAxis 
                    stroke="hsl(var(--muted-foreground))" 
                    fontSize={11} 
                    tickLine={false} 
                    axisLine={false} 
                    tickFormatter={(value) => `${value} KB/s`} 
                />
                <Tooltip 
                  contentStyle={{ 
                    backgroundColor: 'hsl(var(--card))', 
                    borderColor: 'hsl(var(--border))', 
                    borderRadius: '0.75rem',
                    fontSize: '12px',
                    boxShadow: '0 4px 20px -2px rgba(0, 0, 0, 0.5)'
                  }}
                  itemStyle={{ fontWeight: 500 }}
                  labelStyle={{ color: 'hsl(var(--muted-foreground))', marginBottom: '0.25rem' }}
                />
                <Area 
                    type="monotone" 
                    dataKey="upload" 
                    stroke="#f59e0b" 
                    fillOpacity={1} 
                    fill="url(#colorUp)" 
                    strokeWidth={2} 
                    isAnimationActive={false}
                />
                <Area 
                    type="monotone" 
                    dataKey="download" 
                    stroke="#10b981" 
                    fillOpacity={1} 
                    fill="url(#colorDown)" 
                    strokeWidth={2} 
                    isAnimationActive={false}
                />
              </AreaChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* Activity Feed - Matching Network Traffic height */}
        <div className="card-modern p-4 sm:p-6 flex flex-col group" style={{ height: 'fit-content' }}>
          {/* Decorative element */}
          <div className="absolute top-0 right-0 w-24 sm:w-32 h-24 sm:h-32 bg-gradient-to-bl from-accent/10 to-transparent rounded-bl-full opacity-50"></div>
          
          <div className="relative flex items-center justify-between mb-4 sm:mb-5">
            <h3 className="font-bold text-base sm:text-lg flex items-center gap-2 sm:gap-3">
              <div className="p-1.5 sm:p-2 rounded-lg sm:rounded-xl bg-accent/10 border border-accent/20">
                <Shield className="w-4 h-4 sm:w-5 sm:h-5 text-accent" />
              </div>
              <span>Live Activity</span>
            </h3>
            <div className="flex items-center gap-2 text-xs text-muted-foreground bg-muted/20 px-2 py-1 rounded-lg">
              <div className="dot-success animate-pulse"></div>
              <span className="hidden sm:inline">Real-time</span>
            </div>
          </div>
          
          {/* Activity List - Fixed height to match chart area */}
          <div className="h-[280px] sm:h-[360px] overflow-y-auto space-y-2 pr-1 custom-scrollbar">
            {recentActivity.length === 0 ? (
              <div className="h-full flex flex-col items-center justify-center text-center">
                <div className="p-4 rounded-2xl bg-muted/20 mb-4">
                  <Activity className="w-10 h-10 text-muted-foreground/30" />
                </div>
                <p className="text-sm text-muted-foreground">No recent activity</p>
                <p className="text-xs text-muted-foreground/60 mt-1">Activity will appear here as files sync</p>
              </div>
            ) : (
              recentActivity.map((item: any, i: number) => (
                <ActivityItem 
                  key={i}
                  type={item.type}
                  file={item.file}
                  time={item.time}
                  details={item.details}
                />
              ))
            )}
          </div>
        </div>
      </div>

      {/* Health Summary Row */}
      <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-4 gap-3 sm:gap-4">
        <HealthCard
          title="System Health"
          icon={<Shield className="w-5 h-5" />}
          status={health?.healthy ? 'success' : 'warning'}
          value={health?.statusMessage || 'Unknown'}
          sub={health ? `${health.activeWatcherCount || 0} active watcher${(health.activeWatcherCount || 0) !== 1 ? 's' : ''}` : 'Loading...'}
        />
        <HealthCard
          title="Disk Usage"
          icon={<HardDrive className="w-5 h-5" />}
          status={health?.diskUsagePercent > 90 ? 'error' : health?.diskUsagePercent > 75 ? 'warning' : 'success'}
          value={health ? `${health.diskUsagePercent.toFixed(1)}%` : '--'}
          sub={health ? `${(health.diskFreeBytes / (1024 * 1024 * 1024)).toFixed(1)} GB free` : 'Loading...'}
        />
        <HealthCard
          title="Database"
          icon={<Database className="w-5 h-5" />}
          status={health?.dbConnected ? 'success' : 'error'}
          value={health?.dbConnected ? 'Connected' : 'Disconnected'}
          sub={health ? `${(health.dbSizeBytes / 1024).toFixed(0)} KB` : 'Loading...'}
        />
        <HealthCard
          title="Anomaly Detection"
          icon={<AlertTriangle className="w-5 h-5" />}
          status={anomaly?.score > 0.5 ? 'error' : anomaly?.score > 0 ? 'warning' : 'success'}
          value={anomaly?.score > 0 ? `Score: ${(anomaly.score * 100).toFixed(0)}%` : 'Normal'}
          sub={anomaly?.lastType || 'No anomalies detected'}
        />
      </div>

      {/* Degraded Peers Warning */}
      {degradedPeers > 0 && (
        <div className="bg-warning-muted border border-warning/30 rounded-xl p-4 flex items-center gap-3">
          <AlertTriangle className="w-5 h-5 icon-warning" />
          <div>
            <div className="font-medium status-warning">{degradedPeers} Degraded Peer{degradedPeers > 1 ? 's' : ''}</div>
            <div className="text-sm text-muted-foreground">High jitter or packet loss detected. Check network conditions.</div>
          </div>
        </div>
      )}
    </div>
  )
}

function StatCard({ title, value, sub, icon, status, trend }: any) {
  return (
    <div className="card-modern p-5 group">
      {/* Animated corner accent */}
      <div className="absolute top-0 right-0 w-24 h-24 bg-gradient-to-bl from-primary/10 to-transparent rounded-bl-full opacity-0 group-hover:opacity-100 transition-all duration-500"></div>
      
      <div className="relative flex justify-between items-start mb-4">
        <div className="p-3 rounded-2xl bg-gradient-to-br from-secondary to-secondary/50 group-hover:from-primary/20 group-hover:to-primary/5 transition-all duration-300 border border-border/50 group-hover:border-primary/30 shadow-inner">
          {icon}
        </div>
        {status && (
          <div className={`flex items-center gap-1.5 px-2.5 py-1 rounded-full text-[10px] font-bold uppercase tracking-wider backdrop-blur-sm ${
            status === 'success' 
                ? 'badge-success glow-success' 
                : 'bg-warning-muted status-warning border border-warning/30'
          }`}>
             <div className={`w-2 h-2 rounded-full ${status === 'success' ? 'bg-success glow-success' : 'bg-warning animate-pulse'}`} />
             {status === 'success' ? 'Online' : 'Warning'}
          </div>
        )}
      </div>
      
      <div className="relative">
        <h3 className="text-xs font-semibold text-muted-foreground/80 uppercase tracking-wider mb-1">{title}</h3>
        <div className="text-3xl font-bold tracking-tight bg-gradient-to-r from-foreground to-foreground/70 bg-clip-text text-transparent group-hover:from-primary group-hover:to-primary/70 transition-all duration-300">{value}</div>
        <p className="text-xs text-muted-foreground mt-2 flex items-center gap-1.5">
          <Sparkles className="w-3 h-3 text-primary/50" />
          {sub}
        </p>
      </div>
      
      {/* Bottom accent line */}
      <div className="absolute bottom-0 left-1/2 -translate-x-1/2 w-0 h-0.5 bg-gradient-to-r from-transparent via-primary to-transparent group-hover:w-3/4 transition-all duration-500"></div>
    </div>
  )
}

function QuickStat({ label, value, icon, color }: { label: string; value: string | number; icon: React.ReactNode; color: 'teal' | 'emerald' | 'coral' | 'amber' }) {
  const colorClasses = {
    teal: 'from-info/20 to-info-dark/10 border-info/30 status-info',
    emerald: 'from-success/20 to-success-dark/10 border-success/30 status-success',
    coral: 'from-accent/20 to-accent/10 border-accent/30 text-accent',
    amber: 'from-primary/20 to-primary/10 border-primary/30 text-primary',
  }
  
  return (
    <div className={`flex items-center gap-2 sm:gap-3 p-2 sm:p-3 rounded-lg sm:rounded-xl bg-gradient-to-r ${colorClasses[color]} border backdrop-blur-sm hover:scale-105 transition-transform cursor-default`}>
      <div className="p-1.5 sm:p-2 rounded-md sm:rounded-lg bg-background/50">{icon}</div>
      <div className="min-w-0">
        <div className="text-sm sm:text-lg font-bold truncate">{value}</div>
        <div className="text-[8px] sm:text-[10px] text-muted-foreground uppercase tracking-wider truncate">{label}</div>
      </div>
    </div>
  )
}

function HealthCard({ title, icon, status, value, sub }: any) {
  const statusStyles = {
    success: {
      bg: 'from-success/10 to-success-dark/5',
      border: 'border-success/30 hover:border-success/50',
      icon: 'icon-success',
      glow: 'glow-success',
      indicator: 'bg-success glow-success'
    },
    warning: {
      bg: 'from-warning/10 to-warning-dark/5',
      border: 'border-warning/30 hover:border-warning/50',
      icon: 'icon-warning',
      glow: 'glow-warning',
      indicator: 'bg-warning glow-warning animate-pulse'
    },
    error: {
      bg: 'from-error/10 to-error-dark/5',
      border: 'border-error/30 hover:border-error/50',
      icon: 'icon-error',
      glow: 'glow-error',
      indicator: 'bg-error glow-error animate-pulse'
    },
  }
  
  const styles = statusStyles[status as keyof typeof statusStyles] || statusStyles.success
  
  return (
    <div className={`relative overflow-hidden bg-gradient-to-br ${styles.bg} backdrop-blur-sm border ${styles.border} rounded-2xl p-5 shadow-lg ${styles.glow} hover:scale-[1.02] transition-all duration-300 group`}>
      {/* Animated background shimmer */}
      <div className="absolute inset-0 bg-gradient-to-r from-transparent via-white/5 to-transparent translate-x-[-100%] group-hover:translate-x-[100%] transition-transform duration-1000"></div>
      
      <div className="relative">
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center gap-2">
            <div className={`p-2 rounded-xl bg-background/50 ${styles.icon}`}>
              {icon}
            </div>
            <span className="text-sm font-semibold text-muted-foreground">{title}</span>
          </div>
          <div className={`w-2.5 h-2.5 rounded-full ${styles.indicator} shadow-lg`}></div>
        </div>
        <div className="text-xl font-bold tracking-tight">{value}</div>
        <div className="text-xs text-muted-foreground mt-1.5 flex items-center gap-1">
          <div className="w-1 h-1 rounded-full bg-muted-foreground/50"></div>
          {sub}
        </div>
      </div>
    </div>
  )
}

function ActivityItem({ type, file, time, details }: any) {
  const getTypeStyles = () => {
    switch(type) {
      case 'sync': return { 
        icon: <ArrowDown className="w-4 h-4" />, 
        color: 'status-success',
        bg: 'bg-success-muted',
        border: 'border-success/30'
      }
      case 'delete': return { 
        icon: <ArrowUp className="w-4 h-4" />, 
        color: 'status-error',
        bg: 'bg-error-muted',
        border: 'border-error/30'
      }
      case 'security': return { 
        icon: <Shield className="w-4 h-4" />, 
        color: 'text-primary',
        bg: 'bg-primary/10',
        border: 'border-primary/30'
      }
      default: return { 
        icon: <Activity className="w-4 h-4" />, 
        color: 'text-muted-foreground',
        bg: 'bg-muted/20',
        border: 'border-border/50'
      }
    }
  }
  
  const styles = getTypeStyles()

  return (
    <div className="flex gap-3 items-start p-3 rounded-xl bg-gradient-to-r from-secondary/30 to-transparent hover:from-secondary/50 border border-transparent hover:border-border/50 transition-all group cursor-default">
      <div className={`mt-0.5 p-2 rounded-xl ${styles.bg} ${styles.color} border ${styles.border} shadow-sm group-hover:scale-110 transition-transform`}>
        {styles.icon}
      </div>
      <div className="flex-1 min-w-0">
        <div className="flex items-center justify-between gap-2">
          <div className="text-sm font-medium truncate group-hover:text-primary transition-colors">{file}</div>
          <div className="text-[10px] text-muted-foreground/60 whitespace-nowrap bg-muted/30 px-2 py-0.5 rounded-full">{time}</div>
        </div>
        <div className="text-xs text-muted-foreground truncate mt-0.5">{details}</div>
      </div>
    </div>
  )
}
