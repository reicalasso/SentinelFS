import { Activity, ArrowDown, ArrowUp, HardDrive, Shield, Wifi, Zap, Network } from 'lucide-react'
import { Area, AreaChart, CartesianGrid, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts'
import { useState, useEffect } from 'react'

export function Dashboard({ metrics, syncStatus, peersCount, activity }: any) {
  const [trafficHistory, setTrafficHistory] = useState<any[]>([])
  const [lastMetrics, setLastMetrics] = useState<any>(null)
  
  // Format metrics from daemon
  const totalUploadedMB = metrics?.totalUploaded ? (metrics.totalUploaded / (1024 * 1024)).toFixed(2) : '0'
  const totalDownloadedMB = metrics?.totalDownloaded ? (metrics.totalDownloaded / (1024 * 1024)).toFixed(2) : '0'
  const recentActivity = activity || []
  
  // Track network traffic over time
  useEffect(() => {
    if (!metrics) return
    
    const now = new Date()
    const timeStr = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`
    
    // Calculate delta (KB/s)
    let uploadRate = 0
    let downloadRate = 0
    
    if (lastMetrics) {
      const timeDiff = 2 // seconds (polling interval)
      uploadRate = Math.round((metrics.totalUploaded - lastMetrics.totalUploaded) / 1024 / timeDiff)
      downloadRate = Math.round((metrics.totalDownloaded - lastMetrics.totalDownloaded) / 1024 / timeDiff)
    }
    
    setLastMetrics(metrics)
    
    setTrafficHistory(prev => {
      const newHistory = [...prev, { time: timeStr, upload: uploadRate, download: downloadRate }]
      // Keep last 20 data points
      return newHistory.slice(-30)
    })
  }, [metrics])

  return (
    <div className="space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
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
          status={syncStatus?.syncStatus === 'Synced' ? "success" : "warning"}
        />
        <StatCard 
          title="Active Peers" 
          value={`${peersCount || 0} Devices`} 
          sub="Mesh Swarm"
          icon={<Network className="w-5 h-5 text-primary" />}
        />
        <StatCard 
          title="Total Traffic" 
          value={`${totalDownloadedMB} MB`} 
          sub={`Up: ${totalUploadedMB} MB`}
          icon={<HardDrive className="w-5 h-5 text-violet-500" />}
        />
      </div>

      {/* Main Content Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        
        {/* Chart Section */}
        <div className="lg:col-span-2 bg-card/50 backdrop-blur-sm border border-border/50 rounded-xl p-6 shadow-sm hover:border-border/80 transition-colors">
          <div className="flex items-center justify-between mb-6">
            <div>
              <h3 className="font-semibold text-lg flex items-center gap-2">
                <Activity className="w-5 h-5 text-primary" />
                Network Traffic
              </h3>
              <p className="text-sm text-muted-foreground">Real-time bandwidth usage (KB/s)</p>
            </div>
            <div className="flex gap-4 text-xs font-medium">
              <div className="flex items-center gap-2 bg-blue-500/10 px-2 py-1 rounded-md border border-blue-500/20 text-blue-500">
                <div className="w-2 h-2 rounded-full bg-blue-500"></div>
                <span>Upload</span>
              </div>
              <div className="flex items-center gap-2 bg-emerald-500/10 px-2 py-1 rounded-md border border-emerald-500/20 text-emerald-500">
                <div className="w-2 h-2 rounded-full bg-emerald-500"></div>
                <span>Download</span>
              </div>
            </div>
          </div>
          
          <div className="h-[320px] w-full">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={trafficHistory.length > 0 ? trafficHistory : [{ time: 'Now', upload: 0, download: 0 }]}>
                <defs>
                  <linearGradient id="colorUp" x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%" stopColor="#3b82f6" stopOpacity={0.3}/>
                    <stop offset="95%" stopColor="#3b82f6" stopOpacity={0}/>
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
                    stroke="#3b82f6" 
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

        {/* Activity Feed */}
        <div className="bg-card/50 backdrop-blur-sm border border-border/50 rounded-xl p-6 shadow-sm hover:border-border/80 transition-colors flex flex-col h-full">
          <h3 className="font-semibold text-lg mb-4 flex items-center gap-2">
            <Shield className="w-5 h-5 text-primary" />
            Recent Activity
          </h3>
          <div className="flex-1 overflow-y-auto space-y-3 pr-2 custom-scrollbar">
            {recentActivity.length === 0 ? (
              <div className="h-full flex flex-col items-center justify-center text-center text-muted-foreground/50">
                <Activity className="w-12 h-12 mx-auto mb-3 opacity-20" />
                <p className="text-sm">No recent activity detected</p>
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
    </div>
  )
}

function StatCard({ title, value, sub, icon, status, trend }: any) {
  return (
    <div className="bg-card/50 backdrop-blur-sm border border-border/50 rounded-xl p-5 shadow-sm hover:shadow-md hover:border-primary/20 transition-all group relative overflow-hidden">
      <div className="absolute inset-0 bg-gradient-to-br from-primary/5 to-transparent opacity-0 group-hover:opacity-100 transition-opacity duration-500"></div>
      
      <div className="relative flex justify-between items-start mb-3">
        <div className="p-2.5 rounded-xl bg-secondary/50 group-hover:bg-background transition-colors border border-transparent group-hover:border-border/50">
          {icon}
        </div>
        {status && (
          <div className={`flex items-center gap-1.5 px-2 py-0.5 rounded-full text-[10px] font-bold uppercase tracking-wider ${
            status === 'success' 
                ? 'bg-emerald-500/10 text-emerald-500 border border-emerald-500/20' 
                : 'bg-amber-500/10 text-amber-500 border border-amber-500/20 animate-pulse'
          }`}>
             <div className={`w-1.5 h-1.5 rounded-full ${status === 'success' ? 'bg-emerald-500' : 'bg-amber-500'}`} />
             {status === 'success' ? 'OK' : 'WARN'}
          </div>
        )}
      </div>
      
      <div className="relative">
        <h3 className="text-sm font-medium text-muted-foreground">{title}</h3>
        <div className="text-2xl font-bold tracking-tight text-foreground mt-1">{value}</div>
        <p className="text-xs text-muted-foreground mt-1.5 flex items-center gap-1">
            {sub}
        </p>
      </div>
    </div>
  )
}

function ActivityItem({ type, file, time, details }: any) {
  const getIcon = () => {
    switch(type) {
      case 'sync': return <ArrowDown className="w-3.5 h-3.5 text-emerald-500" />
      case 'delete': return <ArrowUp className="w-3.5 h-3.5 text-red-500" />
      case 'security': return <Shield className="w-3.5 h-3.5 text-primary" />
      default: return <Activity className="w-3.5 h-3.5 text-muted-foreground" />
    }
  }

  return (
    <div className="flex gap-3 items-start p-3 rounded-lg bg-secondary/20 hover:bg-secondary/40 border border-transparent hover:border-border/50 transition-all group">
      <div className="mt-1 p-1.5 rounded-full bg-background border border-border/50 shadow-sm">
        {getIcon()}
      </div>
      <div className="flex-1 min-w-0">
        <div className="flex items-center justify-between gap-2">
             <div className="text-sm font-medium truncate text-foreground/90 group-hover:text-primary transition-colors">{file}</div>
             <div className="text-[10px] text-muted-foreground/60 whitespace-nowrap">{time}</div>
        </div>
        <div className="text-xs text-muted-foreground truncate">{details}</div>
      </div>
    </div>
  )
}
