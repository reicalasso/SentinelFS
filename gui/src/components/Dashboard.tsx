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
    <div className="space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
      {/* Hero Section with Animated Stats */}
      <div className="relative overflow-hidden rounded-3xl bg-gradient-to-br from-primary/20 via-card to-accent/10 border border-primary/20 p-8 mb-8">
        {/* Animated Background Effects */}
        <div className="absolute inset-0 overflow-hidden">
          <div className="absolute -top-1/2 -left-1/2 w-full h-full bg-gradient-to-br from-primary/30 to-transparent rounded-full blur-3xl animate-pulse"></div>
          <div className="absolute -bottom-1/2 -right-1/2 w-full h-full bg-gradient-to-tl from-accent/20 to-transparent rounded-full blur-3xl animate-pulse" style={{animationDelay: '1s'}}></div>
          {/* Grid Pattern Overlay */}
          <div className="absolute inset-0 bg-[linear-gradient(rgba(255,255,255,0.02)_1px,transparent_1px),linear-gradient(90deg,rgba(255,255,255,0.02)_1px,transparent_1px)] bg-[size:50px_50px]"></div>
        </div>
        
        <div className="relative z-10">
          <div className="flex items-center gap-3 mb-4">
            <div className="p-3 rounded-2xl bg-primary/20 backdrop-blur-sm border border-primary/30 shadow-lg shadow-primary/20">
              <Shield className="w-8 h-8 text-primary" />
            </div>
            <div>
              <h1 className="text-2xl font-bold bg-gradient-to-r from-foreground to-foreground/60 bg-clip-text text-transparent">
                System Overview
              </h1>
              <p className="text-sm text-muted-foreground flex items-center gap-2">
                <span className="w-2 h-2 rounded-full bg-emerald-500 animate-pulse"></span>
                All systems operational
              </p>
            </div>
          </div>
          
          {/* Quick Stats Row */}
          <div className="grid grid-cols-4 gap-4 mt-6">
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
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        <StatCard 
          title="Network Status" 
          value={metrics ? "Online" : "Connecting"}
          sub="Direct P2P Mesh"
          icon={<Wifi className="w-5 h-5 text-emerald-500" />}
          status={metrics ? "success" : "warning"}
          trend="Stable"
        />
        <StatCard 
          title="Sync Status" 
          value={syncStatus?.syncStatus || "Idle"} 
          sub={syncStatus?.pendingFiles ? `${syncStatus.pendingFiles} files pending` : "Up to date"}
          icon={<Zap className="w-5 h-5 text-amber-500" />}
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
          icon={<HardDrive className="w-5 h-5 text-amber-500" />}
        />
      </div>

      {/* Main Content Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        
        {/* Chart Section */}
        <div className="lg:col-span-2 card-modern p-6 group">
          {/* Decorative top gradient */}
          <div className="absolute top-0 left-0 right-0 h-32 bg-gradient-to-b from-primary/5 to-transparent pointer-events-none"></div>
          
          <div className="relative flex items-center justify-between mb-6">
            <div>
              <h3 className="font-bold text-xl flex items-center gap-3">
                <div className="p-2 rounded-xl bg-primary/10 border border-primary/20">
                  <Activity className="w-5 h-5 text-primary" />
                </div>
                <span className="bg-gradient-to-r from-foreground to-foreground/60 bg-clip-text text-transparent">
                  Network Traffic
                </span>
              </h3>
              <p className="text-sm text-muted-foreground mt-1 ml-12">Real-time bandwidth monitoring</p>
            </div>
            <div className="flex gap-3 text-xs font-semibold">
              <div className="flex items-center gap-2 bg-amber-500/15 px-3 py-1.5 rounded-lg border border-amber-500/30 text-amber-400 shadow-sm shadow-amber-500/10">
                <div className="w-2 h-2 rounded-full bg-amber-400 shadow-sm shadow-amber-400"></div>
                <span>Upload</span>
              </div>
              <div className="flex items-center gap-2 bg-emerald-500/15 px-3 py-1.5 rounded-lg border border-emerald-500/30 text-emerald-400 shadow-sm shadow-emerald-500/10">
                <div className="w-2 h-2 rounded-full bg-emerald-400 shadow-sm shadow-emerald-400"></div>
                <span>Download</span>
              </div>
            </div>
          </div>
          
          {/* Traffic Summary */}
          <div className="relative grid grid-cols-4 gap-3 mb-6">
            <div className="bg-gradient-to-br from-background/80 to-background/40 rounded-xl p-4 border border-border/30 hover:border-emerald-500/30 transition-colors group/stat">
              <div className="text-xs text-muted-foreground mb-1.5 flex items-center gap-1.5">
                <ArrowDown className="w-3 h-3 text-emerald-400" />
                Total Downloaded
              </div>
              <div className="text-xl font-bold text-emerald-400 group-hover/stat:scale-105 transition-transform">{formatBytes(totalDownloaded)}</div>
            </div>
            <div className="bg-gradient-to-br from-background/80 to-background/40 rounded-xl p-4 border border-border/30 hover:border-amber-500/30 transition-colors group/stat">
              <div className="text-xs text-muted-foreground mb-1.5 flex items-center gap-1.5">
                <ArrowUp className="w-3 h-3 text-amber-400" />
                Total Uploaded
              </div>
              <div className="text-xl font-bold text-amber-400 group-hover/stat:scale-105 transition-transform">{formatBytes(totalUploaded)}</div>
            </div>
            <div className="bg-gradient-to-br from-background/80 to-background/40 rounded-xl p-4 border border-border/30 hover:border-emerald-500/30 transition-colors group/stat">
              <div className="text-xs text-muted-foreground mb-1.5 flex items-center gap-1.5">
                <Zap className="w-3 h-3 text-emerald-300" />
                Peak Download
              </div>
              <div className="text-xl font-bold text-emerald-300 group-hover/stat:scale-105 transition-transform">{formatBytes(peakDownload)}/s</div>
            </div>
            <div className="bg-gradient-to-br from-background/80 to-background/40 rounded-xl p-4 border border-border/30 hover:border-amber-500/30 transition-colors group/stat">
              <div className="text-xs text-muted-foreground mb-1.5 flex items-center gap-1.5">
                <Zap className="w-3 h-3 text-amber-300" />
                Peak Upload
              </div>
              <div className="text-xl font-bold text-amber-300 group-hover/stat:scale-105 transition-transform">{formatBytes(peakUpload)}/s</div>
            </div>
          </div>
          
          <div className="h-[240px] w-full relative">
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
        <div className="card-modern p-6 flex flex-col group" style={{ height: 'fit-content' }}>
          {/* Decorative element */}
          <div className="absolute top-0 right-0 w-32 h-32 bg-gradient-to-bl from-accent/10 to-transparent rounded-bl-full opacity-50"></div>
          
          <div className="relative flex items-center justify-between mb-5">
            <h3 className="font-bold text-lg flex items-center gap-3">
              <div className="p-2 rounded-xl bg-accent/10 border border-accent/20">
                <Shield className="w-5 h-5 text-accent" />
              </div>
              <span>Live Activity</span>
            </h3>
            <div className="flex items-center gap-2 text-xs text-muted-foreground bg-muted/20 px-2 py-1 rounded-lg">
              <div className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse"></div>
              Real-time
            </div>
          </div>
          
          {/* Activity List - Fixed height to match chart area */}
          <div className="h-[360px] overflow-y-auto space-y-2 pr-1 custom-scrollbar">
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
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
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
        <div className="bg-amber-500/10 border border-amber-500/30 rounded-xl p-4 flex items-center gap-3">
          <AlertTriangle className="w-5 h-5 text-amber-500" />
          <div>
            <div className="font-medium text-amber-500">{degradedPeers} Degraded Peer{degradedPeers > 1 ? 's' : ''}</div>
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
                ? 'bg-emerald-500/15 text-emerald-400 border border-emerald-500/30 shadow-sm shadow-emerald-500/20' 
                : 'bg-amber-500/15 text-amber-400 border border-amber-500/30 shadow-sm shadow-amber-500/20'
          }`}>
             <div className={`w-2 h-2 rounded-full ${status === 'success' ? 'bg-emerald-400 shadow-sm shadow-emerald-400' : 'bg-amber-400 animate-pulse'}`} />
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
    teal: 'from-teal-500/20 to-teal-600/10 border-teal-500/30 text-teal-400',
    emerald: 'from-emerald-500/20 to-emerald-600/10 border-emerald-500/30 text-emerald-400',
    coral: 'from-rose-500/20 to-rose-600/10 border-rose-500/30 text-rose-400',
    amber: 'from-amber-500/20 to-amber-600/10 border-amber-500/30 text-amber-400',
  }
  
  return (
    <div className={`flex items-center gap-3 p-3 rounded-xl bg-gradient-to-r ${colorClasses[color]} border backdrop-blur-sm hover:scale-105 transition-transform cursor-default`}>
      <div className="p-2 rounded-lg bg-background/50">{icon}</div>
      <div>
        <div className="text-lg font-bold">{value}</div>
        <div className="text-[10px] text-muted-foreground uppercase tracking-wider">{label}</div>
      </div>
    </div>
  )
}

function HealthCard({ title, icon, status, value, sub }: any) {
  const statusStyles = {
    success: {
      bg: 'from-emerald-500/10 to-emerald-600/5',
      border: 'border-emerald-500/30 hover:border-emerald-400/50',
      icon: 'text-emerald-400',
      glow: 'shadow-emerald-500/10',
      indicator: 'bg-emerald-400 shadow-emerald-400/50'
    },
    warning: {
      bg: 'from-amber-500/10 to-amber-600/5',
      border: 'border-amber-500/30 hover:border-amber-400/50',
      icon: 'text-amber-400',
      glow: 'shadow-amber-500/10',
      indicator: 'bg-amber-400 shadow-amber-400/50 animate-pulse'
    },
    error: {
      bg: 'from-red-500/10 to-red-600/5',
      border: 'border-red-500/30 hover:border-red-400/50',
      icon: 'text-red-400',
      glow: 'shadow-red-500/10',
      indicator: 'bg-red-400 shadow-red-400/50 animate-pulse'
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
        color: 'text-emerald-400',
        bg: 'bg-emerald-500/10',
        border: 'border-emerald-500/30'
      }
      case 'delete': return { 
        icon: <ArrowUp className="w-4 h-4" />, 
        color: 'text-red-400',
        bg: 'bg-red-500/10',
        border: 'border-red-500/30'
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
