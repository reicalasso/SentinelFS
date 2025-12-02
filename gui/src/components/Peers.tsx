import { useState, useEffect } from 'react'
import { Laptop, Smartphone, Globe, Ban, Scan, Loader2, Trash2, Wifi, Network, Signal, Zap, Shield } from 'lucide-react'
import { Area, AreaChart, ResponsiveContainer, Tooltip } from 'recharts'

export function Peers({ peers }: { peers?: any[] }) {
  const [isDiscovering, setIsDiscovering] = useState(false)
  const [discoverySettings, setDiscoverySettings] = useState({ udp: true, tcp: false })
  const [relayStatus, setRelayStatus] = useState({ enabled: false, connected: false })
  const [latencyHistory, setLatencyHistory] = useState<Record<string, {time: string, latency: number}[]>>({})
  const [blockedPeers, setBlockedPeers] = useState<Set<string>>(new Set())
  
  // Load discovery settings from localStorage on mount
  useEffect(() => {
    const saved = localStorage.getItem('discoverySettings')
    if (saved) {
      try {
        const parsed = JSON.parse(saved)
        setDiscoverySettings(parsed)
        // If TCP was enabled, check relay status
        if (parsed.tcp && window.api) {
          window.api.sendCommand('GET_RELAY_STATUS').then(res => {
            // Response is JSON string
          })
        }
      } catch {}
    }
  }, [])
  
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

  const toggleSetting = async (key: 'udp' | 'tcp') => {
    const newSettings = { ...discoverySettings, [key]: !discoverySettings[key] }
    setDiscoverySettings(newSettings)
    
    // Persist to localStorage
    localStorage.setItem('discoverySettings', JSON.stringify(newSettings))
    
    // Send to daemon
    if (window.api) {
      const cmd = key === 'udp' 
        ? `SET_DISCOVERY udp=${newSettings.udp ? '1' : '0'}`
        : `SET_DISCOVERY tcp=${newSettings.tcp ? '1' : '0'}`
      await window.api.sendCommand(cmd)
    }
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
        {/* Hero Section */}
        <div className="relative overflow-hidden rounded-3xl bg-gradient-to-br from-violet-500/20 via-card to-primary/10 border border-violet-500/20 p-8">
            {/* Animated Background */}
            <div className="absolute inset-0 overflow-hidden">
                <div className="absolute -top-1/2 -right-1/4 w-96 h-96 bg-violet-500/20 rounded-full blur-3xl animate-pulse"></div>
                <div className="absolute -bottom-1/4 -left-1/4 w-96 h-96 bg-primary/20 rounded-full blur-3xl animate-pulse" style={{animationDelay: '1s'}}></div>
                {/* Network Grid Pattern */}
                <div className="absolute inset-0 bg-[radial-gradient(circle_at_center,rgba(255,255,255,0.03)_1px,transparent_1px)] bg-[size:24px_24px]"></div>
            </div>
            
            <div className="relative z-10 flex items-center justify-between">
                <div className="flex items-center gap-4">
                    <div className="p-4 rounded-2xl bg-gradient-to-br from-violet-500/20 to-primary/20 backdrop-blur-sm border border-violet-500/30 shadow-lg shadow-violet-500/20">
                        <Network className="w-8 h-8 text-violet-400" />
                    </div>
                    <div>
                        <h2 className="text-2xl font-bold bg-gradient-to-r from-foreground to-foreground/60 bg-clip-text text-transparent">
                            Network Mesh
                        </h2>
                        <p className="text-sm text-muted-foreground mt-1 flex items-center gap-2">
                            <span className="w-2 h-2 rounded-full bg-violet-400 animate-pulse"></span>
                            {displayPeers.length} devices connected
                        </p>
                    </div>
                </div>
                
                <div className="flex gap-3">
                    {displayPeers.length > 0 && (
                        <button 
                            onClick={handleClearPeers}
                            className="flex items-center gap-2 bg-destructive/10 hover:bg-destructive/20 text-destructive px-4 py-2.5 rounded-xl text-sm font-medium transition-all border border-destructive/20 hover:border-destructive/40 hover:shadow-lg hover:shadow-destructive/10"
                            title="Clear all peers"
                        >
                            <Trash2 className="w-4 h-4" />
                        </button>
                    )}
                    <button 
                        onClick={handleScan}
                        disabled={isDiscovering}
                        className={`relative overflow-hidden flex items-center gap-2.5 bg-gradient-to-r from-primary to-primary/80 hover:from-primary/90 hover:to-primary/70 text-primary-foreground px-5 py-2.5 rounded-xl text-sm font-semibold transition-all shadow-lg shadow-primary/30 hover:shadow-primary/50 ${isDiscovering ? 'opacity-70 cursor-not-allowed' : ''}`}
                    >
                        {/* Shine effect */}
                        <div className="absolute inset-0 bg-gradient-to-r from-transparent via-white/20 to-transparent translate-x-[-100%] hover:translate-x-[100%] transition-transform duration-500"></div>
                        
                        <span className="relative">
                            {isDiscovering ? <Loader2 className="w-4 h-4 animate-spin" /> : <Scan className="w-4 h-4" />}
                        </span>
                        <span className="relative">{isDiscovering ? 'Scanning...' : 'Scan for Devices'}</span>
                    </button>
                </div>
            </div>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {displayPeers.length === 0 && (
                <div className="col-span-full card-modern p-16 text-center flex flex-col items-center justify-center">
                    <div className="relative mb-6">
                        <div className="absolute inset-0 bg-primary/20 blur-3xl rounded-full"></div>
                        <div className="relative p-6 rounded-3xl bg-gradient-to-br from-secondary to-secondary/50 border border-border/50">
                            <Wifi className="w-12 h-12 text-muted-foreground/40" />
                        </div>
                    </div>
                    <h3 className="font-bold text-xl text-foreground mb-2">No Devices Found</h3>
                    <p className="text-muted-foreground max-w-md">
                        Your mesh network is empty. Click "Scan for Devices" to automatically discover peers on your local network.
                    </p>
                    <button 
                        onClick={handleScan}
                        className="mt-6 px-6 py-3 bg-primary/10 hover:bg-primary/20 text-primary rounded-xl font-medium transition-all border border-primary/20"
                    >
                        Start Discovery
                    </button>
                </div>
            )}
            {displayPeers.map((peer, i) => (
                <div key={i} className="card-modern p-0 group">
                    {/* Status Bar at Top */}
                    <div className={`h-1.5 rounded-t-[20px] transition-all ${
                        peer.status === 'Connected' 
                            ? 'bg-gradient-to-r from-emerald-500 to-emerald-400' 
                            : peer.status === 'Pending' 
                                ? 'bg-gradient-to-r from-amber-500 to-amber-400 animate-pulse' 
                                : 'bg-gradient-to-r from-zinc-600 to-zinc-500'
                    }`}></div>
                    
                    <div className="p-6">
                        <div className="flex items-center gap-4 mb-5">
                            <div className={`relative p-4 rounded-2xl transition-all ${
                                peer.status === 'Connected' 
                                    ? 'bg-gradient-to-br from-emerald-500/20 to-emerald-600/10 text-emerald-400 border border-emerald-500/30 shadow-lg shadow-emerald-500/10' 
                                    : peer.status === 'Pending' 
                                        ? 'bg-gradient-to-br from-amber-500/20 to-amber-600/10 text-amber-400 border border-amber-500/30' 
                                        : 'bg-secondary text-muted-foreground border border-border/50'
                            }`}>
                                {peer.type === 'mobile' ? <Smartphone className="w-6 h-6" /> : <Laptop className="w-6 h-6" />}
                                {peer.status === 'Connected' && (
                                    <div className="absolute -top-1 -right-1 w-3 h-3 rounded-full bg-emerald-400 shadow-lg shadow-emerald-400/50">
                                        <div className="absolute inset-0 rounded-full bg-emerald-400 animate-ping opacity-50"></div>
                                    </div>
                                )}
                            </div>
                            <div className="min-w-0 flex-1">
                                <h3 className="font-bold text-lg truncate group-hover:text-primary transition-colors" title={peer.name}>{peer.name}</h3>
                                <div className="flex items-center gap-2 text-xs text-muted-foreground font-mono mt-1 bg-secondary/50 px-2 py-1 rounded-lg w-fit">
                                    <Globe className="w-3 h-3" />
                                    {peer.ip}
                                </div>
                            </div>
                        </div>

                        {/* Stats Grid */}
                        <div className="grid grid-cols-2 gap-3 mb-4">
                            <div className="bg-gradient-to-br from-secondary/50 to-secondary/20 rounded-xl p-3 border border-border/30">
                                <div className="text-xs text-muted-foreground mb-1 flex items-center gap-1.5">
                                    <Signal className="w-3 h-3" /> Status
                                </div>
                                <div className={`font-semibold flex items-center gap-2 ${
                                    peer.status === 'Connected' ? 'text-emerald-400' : 
                                    peer.status === 'Pending' ? 'text-amber-400' : 'text-muted-foreground'
                                }`}>
                                    <div className={`w-2 h-2 rounded-full ${
                                        peer.status === 'Connected' ? 'bg-emerald-400' : 
                                        peer.status === 'Pending' ? 'bg-amber-400 animate-pulse' : 'bg-zinc-500'
                                    }`} />
                                    {peer.status}
                                </div>
                            </div>
                            <div className="bg-gradient-to-br from-secondary/50 to-secondary/20 rounded-xl p-3 border border-border/30">
                                <div className="text-xs text-muted-foreground mb-1 flex items-center gap-1.5">
                                    <Zap className="w-3 h-3" /> Latency
                                </div>
                                <div className={`font-semibold font-mono ${
                                    peer.latency < 50 ? 'text-emerald-400' : 
                                    peer.latency < 150 ? 'text-amber-400' : 'text-red-400'
                                }`}>
                                    {peer.latency >= 0 ? `${peer.latency}ms` : 'N/A'}
                                </div>
                            </div>
                        </div>
                        
                        {/* Latency Graph */}
                        {peer.id && latencyHistory[peer.id] && latencyHistory[peer.id].length > 1 && (
                            <div className="mt-4 h-20 w-full bg-gradient-to-br from-background/80 to-background/40 rounded-xl overflow-hidden border border-border/30 p-2">
                                <ResponsiveContainer width="100%" height="100%">
                                    <AreaChart data={latencyHistory[peer.id]}>
                                        <defs>
                                            <linearGradient id={`latency-${i}`} x1="0" y1="0" x2="0" y2="1">
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
                                            fill={`url(#latency-${i})`}
                                            strokeWidth={2}
                                            isAnimationActive={false}
                                        />
                                    </AreaChart>
                                </ResponsiveContainer>
                            </div>
                        )}

                        <div className="mt-5 pt-4 border-t border-border/30">
                            <button 
                                onClick={() => handleBlockPeer(peer.id)}
                                className="w-full py-2.5 text-xs font-semibold bg-gradient-to-r from-secondary to-secondary/50 hover:from-destructive/20 hover:to-destructive/10 hover:text-destructive rounded-xl transition-all flex items-center justify-center gap-2 border border-border/50 hover:border-destructive/30"
                            >
                                <Ban className="w-3.5 h-3.5" /> Block Device
                            </button>
                        </div>
                    </div>
                </div>
            ))}
        </div>

        {/* Discovery Settings - Enhanced */}
        <div className="card-modern p-6 mt-8">
            <div className="flex items-center gap-3 mb-6">
                <div className="p-2.5 rounded-xl bg-gradient-to-br from-primary/20 to-primary/5 border border-primary/20">
                    <Shield className="w-5 h-5 text-primary" />
                </div>
                <div>
                    <h3 className="font-bold text-lg">Discovery Settings</h3>
                    <p className="text-xs text-muted-foreground">Configure how devices find each other</p>
                </div>
            </div>
            
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div className="relative overflow-hidden flex items-center justify-between p-5 bg-gradient-to-br from-secondary/50 to-secondary/20 rounded-2xl border border-border/50 hover:border-primary/30 transition-all group">
                    {/* Background accent */}
                    <div className="absolute top-0 right-0 w-32 h-32 bg-primary/5 rounded-full blur-2xl opacity-0 group-hover:opacity-100 transition-opacity"></div>
                    
                    <div className="relative">
                        <div className="font-semibold flex items-center gap-2">
                            <Wifi className="w-4 h-4 text-primary" />
                            Local Network Discovery
                        </div>
                        <div className="text-xs text-muted-foreground mt-1.5">Automatically find peers on the same Wi-Fi</div>
                    </div>
                    <div 
                        className={`relative w-14 h-7 rounded-full cursor-pointer transition-all duration-300 ${
                            discoverySettings.udp 
                                ? 'bg-gradient-to-r from-primary to-primary/80 shadow-lg shadow-primary/30' 
                                : 'bg-secondary border-2 border-muted-foreground/20'
                        }`}
                        onClick={() => toggleSetting('udp')}
                    >
                        <div className={`absolute top-1 w-5 h-5 bg-white rounded-full transition-all duration-300 shadow-md ${
                            discoverySettings.udp ? 'translate-x-7' : 'translate-x-1'
                        }`}></div>
                    </div>
                </div>
                
                <div className="relative overflow-hidden flex items-center justify-between p-5 bg-gradient-to-br from-secondary/50 to-secondary/20 rounded-2xl border border-border/50 hover:border-accent/30 transition-all group">
                    {/* Background accent */}
                    <div className="absolute top-0 right-0 w-32 h-32 bg-accent/5 rounded-full blur-2xl opacity-0 group-hover:opacity-100 transition-opacity"></div>
                    
                    <div className="relative">
                        <div className="font-semibold flex items-center gap-2">
                            <Globe className="w-4 h-4 text-accent" />
                            Global Relay
                            {discoverySettings.tcp && (
                                <span className={`text-[10px] px-2 py-0.5 rounded-full font-bold ${
                                    relayStatus.connected 
                                        ? 'bg-emerald-500/15 text-emerald-400 border border-emerald-500/30' 
                                        : 'bg-amber-500/15 text-amber-400 border border-amber-500/30 animate-pulse'
                                }`}>
                                    {relayStatus.connected ? 'Connected' : 'Connecting...'}
                                </span>
                            )}
                        </div>
                        <div className="text-xs text-muted-foreground mt-1.5">Connect via relay when direct connection fails</div>
                    </div>
                    <div 
                        className={`relative w-14 h-7 rounded-full cursor-pointer transition-all duration-300 ${
                            discoverySettings.tcp 
                                ? 'bg-gradient-to-r from-accent to-accent/80 shadow-lg shadow-accent/30' 
                                : 'bg-secondary border-2 border-muted-foreground/20'
                        }`}
                        onClick={() => toggleSetting('tcp')}
                    >
                        <div className={`absolute top-1 w-5 h-5 bg-white rounded-full transition-all duration-300 shadow-md ${
                            discoverySettings.tcp ? 'translate-x-7' : 'translate-x-1'
                        }`}></div>
                    </div>
                </div>
            </div>
        </div>
    </div>
  )
}
