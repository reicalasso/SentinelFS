import { Activity, ArrowDown, ArrowUp, FileText, Zap } from 'lucide-react'
import { Area, AreaChart, CartesianGrid, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts'

interface FileActivityItem {
  time: string
  filesUploaded: number
  filesDownloaded: number
  filesSynced?: number
}

interface FileActivityChartProps {
  activityHistory: FileActivityItem[]
  filesSynced: number
  filesUploaded: number
  filesDownloaded: number
  activeTransfers: number
  formatBytes: (bytes: number) => string
}

export function FileActivityChart({ 
  activityHistory, 
  filesSynced,
  filesUploaded,
  filesDownloaded,
  activeTransfers,
  formatBytes 
}: FileActivityChartProps) {
  return (
    <div className="xl:col-span-2 card-modern p-4 sm:p-6 group">
      {/* Decorative top gradient */}
      <div className="chart-top-gradient"></div>
      
      <div className="relative flex flex-col sm:flex-row sm:items-center sm:justify-between gap-3 sm:gap-0 mb-4 sm:mb-6">
        <div>
          <h3 className="chart-title">
            <div className="chart-title-icon">
              <FileText className="w-4 h-4 sm:w-5 sm:h-5 text-primary" />
            </div>
            <span className="chart-title-text">
              File Activity
            </span>
          </h3>
          <p className="chart-subtitle">Real-time file synchronization monitoring</p>
        </div>
        <div className="flex gap-2 sm:gap-3 text-xs font-semibold">
          <div className="chart-legend-upload">
            <div className="dot-primary"></div>
            <span>Uploaded</span>
          </div>
          <div className="chart-legend-download">
            <div className="dot-success"></div>
            <span>Downloaded</span>
          </div>
        </div>
      </div>
      
      {/* File Activity Summary */}
      <div className="relative grid grid-cols-2 lg:grid-cols-4 gap-2 sm:gap-3 mb-4 sm:mb-6">
        <TrafficSummaryCard
          icon={<ArrowDown className="w-3 h-3 icon-success" />}
          label="Downloaded"
          value={filesDownloaded.toString()}
          type="download"
        />
        <TrafficSummaryCard
          icon={<ArrowUp className="w-3 h-3 icon-primary" />}
          label="Uploaded"
          value={filesUploaded.toString()}
          type="upload"
        />
        <TrafficSummaryCard
          icon={<Zap className="w-3 h-3 icon-info" />}
          label="Total Synced"
          value={filesSynced.toString()}
          type="sync"
        />
        <TrafficSummaryCard
          icon={<Activity className="w-3 h-3 icon-warning" />}
          label="Active"
          value={activeTransfers.toString()}
          type="active"
        />
      </div>
      
      <div className="h-[180px] sm:h-[240px] w-full relative">
        {/* Chart glow effect */}
        <div className="chart-glow-effect"></div>
        <ResponsiveContainer width="100%" height="100%">
          <AreaChart data={activityHistory.length > 0 ? activityHistory : [{ time: 'Now', filesUploaded: 0, filesDownloaded: 0 }]}>
            <defs>
              <linearGradient id="colorUp" x1="0" y1="0" x2="0" y2="1">
                <stop offset="5%" stopColor="var(--chart-upload-color)" stopOpacity={0.3}/>
                <stop offset="95%" stopColor="var(--chart-upload-color)" stopOpacity={0}/>
              </linearGradient>
              <linearGradient id="colorDown" x1="0" y1="0" x2="0" y2="1">
                <stop offset="5%" stopColor="var(--chart-download-color)" stopOpacity={0.3}/>
                <stop offset="95%" stopColor="var(--chart-download-color)" stopOpacity={0}/>
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
              tickFormatter={(value) => `${value} files`}
              domain={[0, 'dataMax + 1']}
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
              dataKey="filesUploaded" 
              stroke="var(--chart-upload-color)" 
              fillOpacity={1} 
              fill="url(#colorUp)" 
              strokeWidth={2} 
              isAnimationActive={false}
            />
            <Area 
              type="monotone" 
              dataKey="filesDownloaded" 
              stroke="var(--chart-download-color)" 
              fillOpacity={1} 
              fill="url(#colorDown)" 
              strokeWidth={2} 
              isAnimationActive={false}
            />
          </AreaChart>
        </ResponsiveContainer>
      </div>
    </div>
  )
}

interface TrafficSummaryCardProps {
  icon: React.ReactNode
  label: string
  value: string
  type: 'upload' | 'download' | 'sync' | 'active'
}

function TrafficSummaryCard({ icon, label, value, type }: TrafficSummaryCardProps) {
  const getClassName = () => {
    switch(type) {
      case 'download': return 'traffic-summary-download'
      case 'upload': return 'traffic-summary-upload'
      case 'sync': return 'traffic-summary-sync'
      case 'active': return 'traffic-summary-active'
      default: return ''
    }
  }
  
  const getValueClassName = () => {
    switch(type) {
      case 'download': return 'status-success'
      case 'upload': return 'text-primary'
      case 'sync': return 'text-info'
      case 'active': return 'text-warning'
      default: return ''
    }
  }
  
  return (
    <div className={`traffic-summary-card ${getClassName()}`}>
      <div className="traffic-summary-label">
        {icon}
        <span className="hidden sm:inline">Total </span>{label}
      </div>
      <div className={`traffic-summary-value ${getValueClassName()}`}>
        {value}
      </div>
    </div>
  )
}
