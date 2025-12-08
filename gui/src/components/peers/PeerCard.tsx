import { Laptop, Smartphone, Globe, Ban, Signal, Zap, Network } from 'lucide-react'
import { Area, AreaChart, ResponsiveContainer, Tooltip } from 'recharts'

interface PeerCardProps {
  peer: {
    id: string
    name: string
    ip: string
    type: string
    status: string
    trusted: boolean
    latency: number
    lastSeen: string
    transport?: 'TCP' | 'QUIC' | 'Relay'
  }
  index: number
  latencyHistory?: { time: string; latency: number }[]
  onBlock: (peerId: string) => void
}

export function PeerCard({ peer, index, latencyHistory, onBlock }: PeerCardProps) {
  return (
    <div className="card-modern p-0 group">
      {/* Status Bar at Top */}
      <div className={`peer-card-status-bar ${
        peer.status === 'Connected' 
          ? 'peer-card-status-bar-connected' 
          : peer.status === 'Pending' 
            ? 'peer-card-status-bar-pending' 
            : 'peer-card-status-bar-offline'
      }`}></div>
      
      <div className="p-6">
        <div className="flex items-center gap-4 mb-5">
          <div className={`peer-card-icon ${
            peer.status === 'Connected' 
              ? 'peer-card-icon-connected' 
              : peer.status === 'Pending' 
                ? 'peer-card-icon-pending' 
                : 'peer-card-icon-offline'
          }`}>
            {peer.type === 'mobile' ? <Smartphone className="w-6 h-6" /> : <Laptop className="w-6 h-6" />}
            {peer.status === 'Connected' && (
              <div className="peer-card-online-dot">
                <div className="peer-card-online-dot-ping"></div>
              </div>
            )}
          </div>
          <div className="min-w-0 flex-1">
            <h3 className="peer-card-name" title={peer.name}>{peer.name}</h3>
            <div className="peer-card-ip">
              <Globe className="w-3 h-3" />
              {peer.ip}
            </div>
          </div>
        </div>

        {/* Stats Grid */}
        <div className="grid grid-cols-3 gap-2 mb-4">
          <div className="peer-card-stat">
            <div className="peer-card-stat-label">
              <Signal className="w-3 h-3" /> Status
            </div>
            <div className={`peer-card-stat-value ${
              peer.status === 'Connected' ? 'peer-card-stat-value-success' : 
              peer.status === 'Pending' ? 'peer-card-stat-value-warning' : 'peer-card-stat-value-muted'
            }`}>
              <div className={`peer-card-stat-dot ${
                peer.status === 'Connected' ? 'peer-card-stat-dot-success' : 
                peer.status === 'Pending' ? 'peer-card-stat-dot-warning' : 'peer-card-stat-dot-muted'
              }`} />
              {peer.status}
            </div>
          </div>
          <div className="peer-card-stat">
            <div className="peer-card-stat-label">
              <Zap className="w-3 h-3" /> Latency
            </div>
            <div className={`peer-card-stat-value font-mono ${
              peer.latency < 50 ? 'peer-card-stat-value-success' : 
              peer.latency < 150 ? 'peer-card-stat-value-warning' : 'peer-card-stat-value-error'
            }`}>
              {peer.latency >= 0 ? `${peer.latency}ms` : 'N/A'}
            </div>
          </div>
          <div className="peer-card-stat">
            <div className="peer-card-stat-label">
              <Network className="w-3 h-3" /> Transport
            </div>
            <div className={`peer-card-stat-value text-xs ${
              peer.transport === 'TCP' ? 'text-emerald-400' : 
              peer.transport === 'Relay' ? 'text-violet-400' : 'text-amber-400'
            }`}>
              {peer.transport || 'TCP'}
            </div>
          </div>
        </div>
        
        {/* Latency Graph */}
        {peer.id && latencyHistory && latencyHistory.length > 1 && (
          <div className="peer-card-latency-chart">
            <ResponsiveContainer width="100%" height="100%">
              <AreaChart data={latencyHistory}>
                <defs>
                  <linearGradient id={`latency-${index}`} x1="0" y1="0" x2="0" y2="1">
                    <stop offset="5%" stopColor="#8b5cf6" stopOpacity={0.4}/>
                    <stop offset="95%" stopColor="#8b5cf6" stopOpacity={0}/>
                  </linearGradient>
                </defs>
                <Tooltip 
                  contentStyle={{ 
                    background: 'hsl(var(--card))', 
                    border: '1px solid hsl(var(--border))', 
                    borderRadius: '12px', 
                    fontSize: '12px', 
                    padding: '8px 12px',
                    boxShadow: '0 10px 40px rgba(0,0,0,0.3)'
                  }}
                  formatter={(value: any) => [`${value}ms`, 'Latency']}
                  labelStyle={{display: 'none'}}
                />
                <Area 
                  type="monotone" 
                  dataKey="latency" 
                  stroke="#8b5cf6" 
                  fillOpacity={1} 
                  fill={`url(#latency-${index})`}
                  strokeWidth={2}
                  isAnimationActive={false}
                />
              </AreaChart>
            </ResponsiveContainer>
          </div>
        )}

        <div className="peer-card-actions">
          <button 
            onClick={() => onBlock(peer.id)}
            className="peer-card-block-btn"
          >
            <Ban className="w-3.5 h-3.5" /> Block Device
          </button>
        </div>
      </div>
    </div>
  )
}
