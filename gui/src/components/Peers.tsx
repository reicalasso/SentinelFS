import { Laptop, Smartphone, MoreHorizontal, Globe, ShieldCheck, Ban, Scan, Loader2 } from 'lucide-react'
import { useState } from 'react'

const mockPeers = [
    { name: 'MacBook Pro M2', ip: '192.168.1.45', type: 'laptop', status: 'Connected', trusted: true, lastSeen: 'Now' },
    { name: 'iPhone 15 Pro', ip: '192.168.1.89', type: 'mobile', status: 'Connected', trusted: true, lastSeen: 'Now' },
    { name: 'Ubuntu Server', ip: '10.0.0.5', type: 'server', status: 'Disconnected', trusted: true, lastSeen: '2 hours ago' },
    { name: 'Unknown Device', ip: '192.168.1.112', type: 'unknown', status: 'Pending', trusted: false, lastSeen: '1 min ago' },
]

export function Peers({ peers }: { peers?: any[] }) {
  const [isDiscovering, setIsDiscovering] = useState(false)
  const [discoverySettings, setDiscoverySettings] = useState({ udp: true, tcp: false })

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

  // Map backend peers to UI format or use mock if undefined/empty
  const displayPeers = (peers && peers.length > 0) ? peers.map(p => ({
      name: p.id ? `Device ${p.id.substring(0, 8)}` : 'Unknown Device',
      ip: `${p.ip || 'Unknown'}:${p.port || '?'}`,
      type: 'laptop', // Default icon for now
      status: p.status || 'Disconnected',
      trusted: true, // Assume trusted for synced peers
      lastSeen: p.latency >= 0 ? `${p.latency}ms` : 'Unknown'
  })) : mockPeers

  return (
    <div className="space-y-6 animate-in fade-in duration-500">
        <div className="flex items-center justify-between">
            <div>
                <h2 className="text-lg font-semibold">Network Mesh</h2>
                <p className="text-sm text-muted-foreground">Manage connected devices and discovery settings</p>
            </div>
            <button 
                onClick={handleScan}
                disabled={isDiscovering}
                className={`flex items-center gap-2 bg-secondary hover:bg-secondary/80 text-foreground px-4 py-2 rounded-lg text-sm font-medium transition-colors ${isDiscovering ? 'opacity-50 cursor-not-allowed' : ''}`}
            >
                {isDiscovering ? <Loader2 className="w-4 h-4 animate-spin" /> : <Scan className="w-4 h-4" />}
                {isDiscovering ? 'Scanning...' : 'Scan for Devices'}
            </button>
        </div>

        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {displayPeers.map((peer, i) => (
                <div key={i} className="bg-card border border-border rounded-xl p-6 shadow-sm hover:border-blue-500/30 transition-colors relative group">
                    <div className="absolute top-4 right-4 opacity-0 group-hover:opacity-100 transition-opacity">
                        <button className="p-1 hover:bg-secondary rounded text-muted-foreground">
                            <MoreHorizontal className="w-4 h-4" />
                        </button>
                    </div>
                    
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
                            <span className="text-muted-foreground">Last Seen</span>
                            <span>{peer.lastSeen}</span>
                        </div>
                    </div>

                    <div className="mt-6 flex gap-2">
                        {peer.trusted ? (
                            <button className="flex-1 py-2 text-sm font-medium bg-secondary/50 hover:bg-secondary text-foreground rounded-lg transition-colors border border-transparent hover:border-red-500/20 hover:text-red-500">
                                Block
                            </button>
                        ) : (
                            <div className="flex gap-2 w-full">
                                <button className="flex-1 py-2 text-sm font-medium bg-green-600 hover:bg-green-700 text-white rounded-lg transition-colors flex items-center justify-center gap-1">
                                    <ShieldCheck className="w-3 h-3" /> Trust
                                </button>
                                <button className="flex-1 py-2 text-sm font-medium bg-red-500/10 hover:bg-red-500/20 text-red-500 rounded-lg transition-colors flex items-center justify-center gap-1">
                                    <Ban className="w-3 h-3" /> Block
                                </button>
                            </div>
                        )}
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
