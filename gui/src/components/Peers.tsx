import { useState, useEffect } from 'react'
import { Laptop, Smartphone, Globe, Ban, Scan, Loader2, Trash2, Wifi, Network } from 'lucide-react'
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
    <div className="space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
        <div className="flex items-center justify-between">
            <div>
                <h2 className="text-lg font-semibold flex items-center gap-2">
                    <Network className="w-5 h-5 text-primary" />
                    Network Mesh
                </h2>
                <p className="text-sm text-muted-foreground">Manage connected devices and discovery settings</p>
            </div>
            <div className="flex gap-2">
                {displayPeers.length > 0 && (
                    <button 
                        onClick={handleClearPeers}
                        className="flex items-center gap-2 bg-destructive/10 hover:bg-destructive/20 text-destructive px-3 py-2 rounded-lg text-sm font-medium transition-colors"
                        title="Clear all peers"
                    >
                        <Trash2 className="w-4 h-4" />
                    </button>
                )}
                <button 
                    onClick={handleScan}
                    disabled={isDiscovering}
                    className={`flex items-center gap-2 bg-primary hover:bg-primary/90 text-primary-foreground px-4 py-2 rounded-lg text-sm font-medium transition-colors shadow-sm shadow-primary/20 ${isDiscovering ? 'opacity-50 cursor-not-allowed' : ''}`}
                >
                    {isDiscovering ? <Loader2 className="w-4 h-4 animate-spin" /> : <Scan className="w-4 h-4" />}
                    {isDiscovering ? 'Scanning...' : 'Scan for Devices'}
                </button>
            </div>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {displayPeers.length === 0 && (
                <div className="col-span-full bg-card/50 backdrop-blur-sm border border-dashed border-border rounded-xl p-12 text-center flex flex-col items-center justify-center">
                    <div className="bg-secondary/50 p-4 rounded-full mb-4">
                        <Wifi className="w-8 h-8 text-muted-foreground opacity-50" />
                    </div>
                    <h3 className="font-medium text-lg text-foreground">No Devices Found</h3>
                    <p className="text-muted-foreground max-w-sm mt-2">
                        Your mesh network is empty. Click "Scan for Devices" to automatically discover peers on your local network, or add them manually in Settings.
                    </p>
                </div>
            )}
            {displayPeers.map((peer, i) => (
                <div key={i} className="bg-card/50 backdrop-blur-sm border border-border/50 rounded-xl p-6 shadow-sm hover:shadow-md hover:border-primary/30 transition-all relative group overflow-hidden">
                    <div className="absolute top-0 left-0 w-full h-1 bg-gradient-to-r from-transparent via-primary/50 to-transparent opacity-0 group-hover:opacity-100 transition-opacity"></div>
                    
                    <div className="flex items-center gap-4 mb-4">
                        <div className={`p-3 rounded-xl ${
                            peer.status === 'Connected' ? 'bg-emerald-500/10 text-emerald-500 ring-1 ring-emerald-500/20' : 
                            peer.status === 'Pending' ? 'bg-amber-500/10 text-amber-500 ring-1 ring-amber-500/20' : 'bg-secondary text-muted-foreground'
                        }`}>
                            {peer.type === 'mobile' ? <Smartphone className="w-6 h-6" /> : <Laptop className="w-6 h-6" />}
                        </div>
                        <div className="min-w-0 flex-1">
                            <h3 className="font-semibold truncate" title={peer.name}>{peer.name}</h3>
                            <div className="flex items-center gap-2 text-xs text-muted-foreground font-mono mt-0.5">
                                <Globe className="w-3 h-3" />
                                {peer.ip}
                            </div>
                        </div>
                    </div>

                    <div className="space-y-3">
                        <div className="flex justify-between text-sm">
                            <span className="text-muted-foreground">Status</span>
                            <span className={`font-medium flex items-center gap-1.5 ${
                                peer.status === 'Connected' ? 'text-emerald-500' : 
                                peer.status === 'Pending' ? 'text-amber-500' : 'text-muted-foreground'
                            }`}>
                                <div className={`w-1.5 h-1.5 rounded-full ${
                                    peer.status === 'Connected' ? 'bg-emerald-500' : 
                                    peer.status === 'Pending' ? 'bg-amber-500' : 'bg-zinc-500'
                                }`} />
                                {peer.status}
                            </span>
                        </div>
                        <div className="flex justify-between text-sm">
                            <span className="text-muted-foreground">Latency</span>
                            <span className={`font-mono ${peer.latency < 50 ? 'text-emerald-500' : peer.latency < 150 ? 'text-amber-500' : 'text-red-500'}`}>
                                {peer.latency >= 0 ? `${peer.latency}ms` : 'N/A'}
                            </span>
                        </div>
                    </div>
                    
                    {/* Latency Graph */}
                    {peer.id && latencyHistory[peer.id] && latencyHistory[peer.id].length > 1 && (
                        <div className="mt-4 h-16 w-full bg-background/30 rounded-lg overflow-hidden">
                            <ResponsiveContainer width="100%" height="100%">
                                <AreaChart data={latencyHistory[peer.id]}>
                                    <defs>
                                        <linearGradient id={`latency-${i}`} x1="0" y1="0" x2="0" y2="1">
                                            <stop offset="5%" stopColor="#3b82f6" stopOpacity={0.3}/>
                                            <stop offset="95%" stopColor="#3b82f6" stopOpacity={0}/>
                                        </linearGradient>
                                    </defs>
                                    <Tooltip 
                                        contentStyle={{ background: 'hsl(var(--card))', border: '1px solid hsl(var(--border))', borderRadius: '8px', fontSize: '12px', padding: '4px 8px' }}
                                        formatter={(value: any) => [`${value}ms`, '']}
                                        labelStyle={{display: 'none'}}
                                    />
                                    <Area 
                                        type="monotone" 
                                        dataKey="latency" 
                                        stroke="#3b82f6" 
                                        fillOpacity={1} 
                                        fill={`url(#latency-${i})`}
                                        strokeWidth={2}
                                        isAnimationActive={false}
                                    />
                                </AreaChart>
                            </ResponsiveContainer>
                        </div>
                    )}

                    <div className="mt-4 pt-4 border-t border-border/30">
                        <button 
                            onClick={() => handleBlockPeer(peer.id)}
                            className="w-full py-2 text-xs font-medium bg-secondary hover:bg-destructive/10 hover:text-destructive rounded-lg transition-colors flex items-center justify-center gap-2"
                        >
                            <Ban className="w-3 h-3" /> Block Device
                        </button>
                    </div>
                </div>
            ))}
        </div>

        {/* Discovery Settings */}
        <div className="bg-card/50 backdrop-blur-sm border border-border/50 rounded-xl p-6 mt-8">
            <h3 className="font-semibold mb-4 flex items-center gap-2">
                <Scan className="w-4 h-4 text-primary" />
                Discovery Settings
            </h3>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                <div className="flex items-center justify-between p-4 bg-secondary/20 rounded-xl border border-border/50">
                    <div>
                        <div className="font-medium text-sm">Local Network Discovery (UDP)</div>
                        <div className="text-xs text-muted-foreground mt-1">Automatically find peers on the same Wi-Fi</div>
                    </div>
                    <div 
                        className={`w-11 h-6 rounded-full relative cursor-pointer transition-all duration-300 border-2 ${discoverySettings.udp ? 'bg-primary border-primary' : 'bg-transparent border-muted-foreground/30'}`}
                        onClick={() => toggleSetting('udp')}
                    >
                        <div className={`absolute top-0.5 w-4 h-4 bg-white rounded-full transition-all duration-300 shadow-sm ${discoverySettings.udp ? 'translate-x-5' : 'translate-x-0.5 bg-muted-foreground'}`}></div>
                    </div>
                </div>
                <div className="flex items-center justify-between p-4 bg-secondary/20 rounded-xl border border-border/50">
                    <div>
                        <div className="font-medium text-sm">Global Relay (TCP)</div>
                        <div className="text-xs text-muted-foreground mt-1">Connect via relay when direct connection fails</div>
                    </div>
                    <div 
                        className={`w-11 h-6 rounded-full relative cursor-pointer transition-all duration-300 border-2 ${discoverySettings.tcp ? 'bg-primary border-primary' : 'bg-transparent border-muted-foreground/30'}`}
                        onClick={() => toggleSetting('tcp')}
                    >
                        <div className={`absolute top-0.5 w-4 h-4 bg-white rounded-full transition-all duration-300 shadow-sm ${discoverySettings.tcp ? 'translate-x-5' : 'translate-x-0.5 bg-muted-foreground'}`}></div>
                    </div>
                </div>
            </div>
        </div>
    </div>
  )
}
