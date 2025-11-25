import { Activity, ArrowDown, ArrowUp, HardDrive, Shield, Wifi, Zap } from 'lucide-react'
import { Area, AreaChart, CartesianGrid, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts'
import { useState, useEffect } from 'react'

export function Dashboard({ metrics, syncStatus, peersCount, activity }: any) {
  const [trafficHistory, setTrafficHistory] = useState<any[]>([])
  const [lastMetrics, setLastMetrics] = useState<any>(null)
  
  // Format metrics from daemon
  const totalUploadedMB = metrics?.totalUploaded ? (metrics.totalUploaded / (1024 * 1024)).toFixed(2) : '0'
  const totalDownloadedMB = metrics?.totalDownloaded ? (metrics.totalDownloaded / (1024 * 1024)).toFixed(2) : '0'
  const filesSynced = metrics?.filesSynced || 0
  const recentActivity = activity || []
  
  // Track network traffic over time
  useEffect(() => {
    if (!metrics) return
    
    const now = new Date()
    const timeStr = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}`
    
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
      return newHistory.slice(-20)
    })
  }, [metrics])

  return (
    <div className="space-y-6 animate-in fade-in duration-500">
      {/* Top Stats Row */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        <StatCard 
          title="Network Status" 
          value={metrics ? "Online" : "Connecting..."}
          sub="Direct Connection (P2P)"
          icon={<Wifi className="text-blue-500" />}
          status={metrics ? "success" : "warning"}
        />
        <StatCard 
          title="Sync Status" 
          value={syncStatus?.syncStatus || "Idle"} 
          sub={syncStatus?.pendingFiles ? `${syncStatus.pendingFiles} files pending` : "All files up to date"}
          icon={<Zap className="text-yellow-500" />}
          status={syncStatus?.syncStatus === 'Synced' ? "success" : "warning"}
        />
        <StatCard 
          title="Active Peers" 
          value={`${peersCount || 0} Devices`} 
          sub="Connected to Mesh"
          icon={<Activity className="text-green-500" />}
        />
        <StatCard 
          title="Data Transferred" 
          value={`${totalDownloadedMB} MB`} 
          sub={`Uploaded: ${totalUploadedMB} MB`}
          icon={<HardDrive className="text-purple-500" />}
        />
      </div>

      {/* Main Content Grid */}
      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        
        {/* Chart Section */}
        <div className="lg:col-span-2 bg-card border border-border rounded-xl p-6 shadow-sm">
          <div className="flex items-center justify-between mb-6">
            <div>
              <h3 className="font-semibold text-lg">Network Traffic</h3>
              <p className="text-xs text-muted-foreground">Real-time bandwidth usage</p>
            </div>
            <div className="flex gap-4 text-xs">
              <div className="flex items-center gap-1">
                <div className="w-2 h-2 rounded-full bg-blue-500"></div>
                <span>Upload</span>
              </div>
              <div className="flex items-center gap-1">
                <div className="w-2 h-2 rounded-full bg-emerald-500"></div>
                <span>Download</span>
              </div>
            </div>
          </div>
          
          <div className="h-[300px] w-full">
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
                <CartesianGrid strokeDasharray="3 3" stroke="#333" vertical={false} />
                <XAxis dataKey="time" stroke="#666" fontSize={12} tickLine={false} axisLine={false} />
                <YAxis stroke="#666" fontSize={12} tickLine={false} axisLine={false} tickFormatter={(value) => `${value} MB`} />
                <Tooltip 
                  contentStyle={{ backgroundColor: '#18181b', borderColor: '#333' }}
                  itemStyle={{ fontSize: '12px' }}
                />
                <Area type="monotone" dataKey="upload" stroke="#3b82f6" fillOpacity={1} fill="url(#colorUp)" strokeWidth={2} />
                <Area type="monotone" dataKey="download" stroke="#10b981" fillOpacity={1} fill="url(#colorDown)" strokeWidth={2} />
              </AreaChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* Activity Feed */}
        <div className="bg-card border border-border rounded-xl p-6 shadow-sm flex flex-col">
          <h3 className="font-semibold text-lg mb-4 flex items-center gap-2">
            <Shield className="w-5 h-5 text-blue-500" />
            Recent Activity
          </h3>
          <div className="flex-1 overflow-auto space-y-4 pr-2">
            {recentActivity.length === 0 ? (
              <div className="text-center text-muted-foreground py-8">
                <Activity className="w-8 h-8 mx-auto mb-2 opacity-50" />
                <p className="text-sm">No recent activity</p>
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

function StatCard({ title, value, sub, icon, status }: any) {
  return (
    <div className="bg-card border border-border rounded-xl p-5 shadow-sm hover:border-blue-500/30 transition-all group">
      <div className="flex justify-between items-start mb-2">
        <div className="p-2 rounded-lg bg-secondary group-hover:bg-background transition-colors">
          {icon}
        </div>
        {status && (
          <div className={`w-2 h-2 rounded-full ${
            status === 'success' ? 'bg-green-500 shadow-[0_0_8px_rgba(34,197,94,0.6)]' : 'bg-yellow-500'
          }`} />
        )}
      </div>
      <div className="mt-2">
        <h3 className="text-sm font-medium text-muted-foreground">{title}</h3>
        <div className="text-2xl font-bold tracking-tight text-foreground">{value}</div>
        <p className="text-xs text-muted-foreground mt-1">{sub}</p>
      </div>
    </div>
  )
}

function ActivityItem({ type, file, time, details }: any) {
  const getIcon = () => {
    switch(type) {
      case 'sync': return <ArrowDown className="w-4 h-4 text-green-500" />
      case 'delete': return <ArrowUp className="w-4 h-4 text-red-500" />
      case 'security': return <Shield className="w-4 h-4 text-blue-500" />
      default: return <Activity className="w-4 h-4 text-gray-500" />
    }
  }

  return (
    <div className="flex gap-3 items-start p-3 rounded-lg bg-secondary/30 hover:bg-secondary/50 transition-colors">
      <div className="mt-1">{getIcon()}</div>
      <div>
        <div className="text-sm font-medium">{file}</div>
        <div className="text-xs text-muted-foreground">{details}</div>
        <div className="text-[10px] text-muted-foreground/60 mt-1">{time}</div>
      </div>
    </div>
  )
}
