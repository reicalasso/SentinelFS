import { Activity, ArrowDown, ArrowUp, Zap } from 'lucide-react'
import { Area, AreaChart, CartesianGrid, ResponsiveContainer, Tooltip, XAxis, YAxis } from 'recharts'

interface TrafficHistoryItem {
  time: string
  upload: number
  download: number
  uploadBytes: number
  downloadBytes: number
}

interface NetworkTrafficChartProps {
  trafficHistory: TrafficHistoryItem[]
  totalDownloaded: number
  totalUploaded: number
  peakDownload: number
  peakUpload: number
  formatBytes: (bytes: number) => string
}

export function NetworkTrafficChart({ 
  trafficHistory, 
  totalDownloaded, 
  totalUploaded, 
  peakDownload, 
  peakUpload,
  formatBytes 
}: NetworkTrafficChartProps) {
  return (
    <div className="xl:col-span-2 card-modern p-4 sm:p-6 group">
      {/* Decorative top gradient */}
      <div className="chart-top-gradient"></div>
      
      <div className="relative flex flex-col sm:flex-row sm:items-center sm:justify-between gap-3 sm:gap-0 mb-4 sm:mb-6">
        <div>
          <h3 className="chart-title">
            <div className="chart-title-icon">
              <Activity className="w-4 h-4 sm:w-5 sm:h-5 text-primary" />
            </div>
            <span className="chart-title-text">
              Network Traffic
            </span>
          </h3>
          <p className="chart-subtitle">Real-time bandwidth monitoring</p>
        </div>
        <div className="flex gap-2 sm:gap-3 text-xs font-semibold">
          <div className="chart-legend-upload">
            <div className="dot-primary"></div>
            <span>Upload</span>
          </div>
          <div className="chart-legend-download">
            <div className="dot-success"></div>
            <span>Download</span>
          </div>
        </div>
      </div>
      
      {/* Traffic Summary */}
      <div className="relative grid grid-cols-2 lg:grid-cols-4 gap-2 sm:gap-3 mb-4 sm:mb-6">
        <TrafficSummaryCard
          icon={<ArrowDown className="w-3 h-3 icon-success" />}
          label="Downloaded"
          value={formatBytes(totalDownloaded)}
          type="download"
        />
        <TrafficSummaryCard
          icon={<ArrowUp className="w-3 h-3 icon-primary" />}
          label="Uploaded"
          value={formatBytes(totalUploaded)}
          type="upload"
        />
        <TrafficSummaryCard
          icon={<Zap className="w-3 h-3 icon-success" />}
          label="Peak Download"
          value={`${formatBytes(peakDownload)}/s`}
          type="download"
        />
        <TrafficSummaryCard
          icon={<Zap className="w-3 h-3 icon-primary" />}
          label="Peak Upload"
          value={`${formatBytes(peakUpload)}/s`}
          type="upload"
        />
      </div>
      
      <div className="h-[180px] sm:h-[240px] w-full relative">
        {/* Chart glow effect */}
        <div className="chart-glow-effect"></div>
        <ResponsiveContainer width="100%" height="100%">
          <AreaChart data={trafficHistory.length > 0 ? trafficHistory : [{ time: 'Now', upload: 0, download: 0 }]}>
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
              tickFormatter={(value) => `${value.toFixed(2)} KB/s`}
              domain={[0, 'auto']}
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
              stroke="var(--chart-upload-color)" 
              fillOpacity={1} 
              fill="url(#colorUp)" 
              strokeWidth={2} 
              isAnimationActive={false}
            />
            <Area 
              type="monotone" 
              dataKey="download" 
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
  type: 'upload' | 'download'
}

function TrafficSummaryCard({ icon, label, value, type }: TrafficSummaryCardProps) {
  return (
    <div className={`traffic-summary-card ${type === 'download' ? 'traffic-summary-download' : 'traffic-summary-upload'}`}>
      <div className="traffic-summary-label">
        {icon}
        <span className="hidden sm:inline">Total </span>{label}
      </div>
      <div className={`traffic-summary-value ${type === 'download' ? 'status-success' : 'text-primary'}`}>
        {value}
      </div>
    </div>
  )
}
