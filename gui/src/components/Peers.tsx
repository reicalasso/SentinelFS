import { useState, useEffect } from 'react'
import { Laptop, Smartphone, Globe, ShieldCheck, Ban, Scan, Loader2, Trash2 } from 'lucide-react'
import { Area, AreaChart, ResponsiveContainer, Tooltip } from 'recharts'

export function Peers({ peers }: { peers?: any[] }) {
  const [isDiscovering, setIsDiscovering] = useState(false)
  const [discoverySettings, setDiscoverySettings] = useState({ udp: true, tcp: false })
  const [latencyHistory, setLatencyHistory] = useState<Record<string, {time: string, latency: number}[]>>({})
  const [blockedPeers, setBlockedPeers] = useState<Set<string>>(new Set())
  
  // Track latency history for each peer
  useEffect(() => {
    if (!peers || peers.length === 0) return
    
    const now = new Date()
    const timeStr = `${now.getMinutes()}:${now.getSeconds().toString().padStart(2, '0')}`
    
    setLatencyHistory(prev => {
      const updated = { ...prev }
      peers.forEach(peer => {
        if (peer.id && peer.latency >= 0) {
          const history = updated[peer.id] || []
          updated[peer.id] = [...history.slice(-20), { time: timeStr, latency: peer.latency }]
        }
      })
      return updated
    })
  }, [peers])

  const handleScan = async () => {
    if (window.api) {
      setIsDiscovering(true)
      await window.api.sendCommand('DISCOVER')
      // Reset discovering state after 3 seconds
      setTimeout(() => setIsDiscovering(false), 3000)
    }
  }

  const toggleSetting = (key: 'udp' | 'tcp') => {
    setDiscoverySettings(prev => ({ ...prev, [key]: !prev[key] }))
  }
  
  const handleBlockPeer = async (peerId: string) => {
    if (window.api && peerId) {
      await window.api.sendCommand(`BLOCK_PEER ${peerId}`)
      setBlockedPeers(prev => new Set([...prev, peerId]))
    }
  }
  
  const handleClearPeers = async () => {
    if (window.api && confirm('Clear all peers from database? They will be re-discovered on next scan.')) {
      await window.api.sendCommand('CLEAR_PEERS')
      setLatencyHistory({})
    }
  }

  // Filter out blocked peers and map backend peers to UI format
  const displayPeers = (peers && peers.length > 0) ? peers
      .filter(p => !blockedPeers.has(p.id))
      .map(p => ({
      id: p.id,
      name: p.id ? `Device ${p.id.substring(0, 8)}` : 'Unknown Device',
      ip: `${p.ip || 'Unknown'}:${p.port || '?'}`,
      type: 'laptop', // Default icon for now
      status: p.status || 'Disconnected',
      trusted: true, // Assume trusted for synced peers
      latency: p.latency >= 0 ? p.latency : -1,
      lastSeen: p.latency >= 0 ? `${p.latency}ms` : 'Unknown'
  })) : []

  return (
    <div className="space-y-6 animate-in fade-in duration-500">
        <div className="flex items-center justify-between">
            <div>
                <h2 className="text-lg font-semibold">Network Mesh</h2>
                <p className="text-sm text-muted-foreground">Manage connected devices and discovery settings</p>
            </div>
            <div className="flex gap-2">
                {displayPeers.length > 0 && (
                    <button 
                        onClick={handleClearPeers}
                        className="flex items-center gap-2 bg-red-500/10 hover:bg-red-500/20 text-red-500 px-3 py-2 rounded-lg text-sm font-medium transition-colors"
                        title="Clear all peers"
                    >
                        <Trash2 className="w-4 h-4" />
                    </button>
                )}
                <button 
                    onClick={handleScan}
                    disabled={isDiscovering}
                    className={`flex items-center gap-2 bg-secondary hover:bg-secondary/80 text-foreground px-4 py-2 rounded-lg text-sm font-medium transition-colors ${isDiscovering ? 'opacity-50 cursor-not-allowed' : ''}`}
                >
                    {isDiscovering ? <Loader2 className="w-4 h-4 animate-spin" /> : <Scan className="w-4 h-4" />}
                    {isDiscovering ? 'Scanning...' : 'Scan for Devices'}
                </button>
            </div>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {displayPeers.length === 0 && (
                <div className="col-span-full bg-card border border-border rounded-xl p-8 text-center">
                    <Laptop className="w-12 h-12 mx-auto mb-3 opacity-50 text-muted-foreground" />
                    <p className="text-muted-foreground">No devices found. Click "Scan for Devices" to discover peers on your network.</p>
                </div>
            )}
            {displayPeers.map((peer, i) => (
                <div key={i} className="bg-card border border-border rounded-xl p-6 shadow-sm hover:border-blue-500/30 transition-colors relative group">
                    
                    <div className="flex items-center gap-4 mb-4">
                        <div className={`p-3 rounded-full ${
                            peer.status === 'Connected' ? 'bg-green-500/10 text-green-500' : 
                            peer.status === 'Pending' ? 'bg-yellow-500/10 text-yellow-500' : 'bg-secondary text-muted-foreground'
                        }`}>
                            {peer.type === 'mobile' ? <Smartphone className="w-6 h-6" /> : <Laptop className="w-6 h-6" />}
                        </div>
                        <div>
                            <h3 className="font-semibold">{peer.name}</h3>
                            <div className="flex items-center gap-2 text-xs text-muted-foreground">
                                <Globe className="w-3 h-3" />
                                {peer.ip}
                            </div>
                        </div>
                    </div>

                    <div className="space-y-3">
                        <div className="flex justify-between text-sm">
                            <span className="text-muted-foreground">Status</span>
                            <span className={`font-medium ${
                                peer.status === 'Connected' ? 'text-green-500' : 
                                peer.status === 'Pending' ? 'text-yellow-500' : 'text-muted-foreground'
                            }`}>{peer.status}</span>
                        </div>
                        <div className="flex justify-between text-sm">
                            <span className="text-muted-foreground">Latency</span>
                            <span className={`font-mono ${peer.latency < 50 ? 'text-green-500' : peer.latency < 150 ? 'text-yellow-500' : 'text-red-500'}`}>
                                {peer.latency >= 0 ? `${peer.latency}ms` : 'N/A'}
                            </span>
                        </div>
                    </div>
                    
                    {/* Latency Graph */}
                    {peer.id && latencyHistory[peer.id] && latencyHistory[peer.id].length > 1 && (
                        <div className="mt-4 h-16 w-full">
                            <ResponsiveContainer width="100%" height="100%">
                                <AreaChart data={latencyHistory[peer.id]}>
                                    <defs>
                                        <linearGradient id={`latency-${i}`} x1="0" y1="0" x2="0" y2="1">
                                            <stop offset="5%" stopColor="#3b82f6" stopOpacity={0.3}/>
                                            <stop offset="95%" stopColor="#3b82f6" stopOpacity={0}/>
                                        </linearGradient>
                                    </defs>
                                    <Tooltip 
                                        contentStyle={{ background: 'hsl(var(--card))', border: '1px solid hsl(var(--border))', borderRadius: '8px', fontSize: '12px' }}
                                        formatter={(value: any) => [`${value}ms`, 'Latency']}
                                    />
                                    <Area 
                                        type="monotone" 
                                        dataKey="latency" 
                                        stroke="#3b82f6" 
                                        fillOpacity={1} 
                                        fill={`url(#latency-${i})`}
                                        strokeWidth={2}
                                    />
                                </AreaChart>
                            </ResponsiveContainer>
                        </div>
                    )}

                    <div className="mt-4">
                        <button 
                            onClick={() => handleBlockPeer(peer.id)}
                            className="w-full py-2 text-sm font-medium bg-red-500/10 hover:bg-red-500/20 text-red-500 rounded-lg transition-colors flex items-center justify-center gap-1"
                        >
                            <Ban className="w-3 h-3" /> Block Device
                        </button>
                    </div>
                </div>
            ))}
        </div>

        {/* Discovery Settings */}
        <div className="bg-card border border-border rounded-xl p-6 mt-8">
            <h3 className="font-semibold mb-4">Discovery Settings</h3>
            <div className="flex items-center justify-between py-3 border-b border-border">
                <div>
                    <div className="font-medium text-sm">Local Network Discovery (UDP)</div>
                    <div className="text-xs text-muted-foreground">Automatically find peers on the same Wi-Fi</div>
                </div>
                <div 
                    className={`w-10 h-5 rounded-full relative cursor-pointer transition-colors ${discoverySettings.udp ? 'bg-green-600' : 'bg-secondary'}`}
                    onClick={() => toggleSetting('udp')}
                >
                    <div className={`absolute top-1 w-3 h-3 bg-white rounded-full transition-all ${discoverySettings.udp ? 'right-1' : 'left-1'}`}></div>
                </div>
            </div>
            <div className="flex items-center justify-between py-3">
                <div>
                    <div className="font-medium text-sm">Global Relay (TCP)</div>
                    <div className="text-xs text-muted-foreground">Connect to peers over the internet when direct connection fails</div>
                </div>
                <div 
                    className={`w-10 h-5 rounded-full relative cursor-pointer transition-colors ${discoverySettings.tcp ? 'bg-green-600' : 'bg-secondary'}`}
                    onClick={() => toggleSetting('tcp')}
                >
                    <div className={`absolute top-1 w-3 h-3 bg-white rounded-full transition-all ${discoverySettings.tcp ? 'right-1' : 'left-1'}`}></div>
                </div>
            </div>
        </div>
    </div>
  )
}
