import { useState, useEffect , cloneElement} from 'react'
import { Activity, Radio, Settings, Shield, Terminal, Users, Wifi, Play, Pause, RefreshCw, HardDrive } from 'lucide-react'

// Define the API interface exposed from preload
interface ElectronAPI {
  on: (channel: string, callback: (data: any) => void) => void
  off: (channel: string, callback: (data: any) => void) => void
  sendCommand: (command: string) => Promise<{ success: boolean; error?: string }>
}

declare global {
  interface Window {
    api: ElectronAPI
  }
}

type Tab = 'dashboard' | 'peers' | 'settings' | 'logs'

export default function App() {
  const [activeTab, setActiveTab] = useState<Tab>('dashboard')
  const [status, setStatus] = useState<'connected' | 'disconnected' | 'error'>('disconnected')
  const [logs, setLogs] = useState<string[]>([])
  const [isPaused, setIsPaused] = useState(false)
  const [peers, setPeers] = useState<string[]>([])
  const [bandwidthStats, setBandwidthStats] = useState<any>(null)

  const handleLog = (log: string) => {
    setLogs(prev => [...prev.slice(-99), log]) // Keep last 100 logs
  }

  useEffect(() => {
    // Listen for daemon status
    const handleStatus = (newStatus: any) => setStatus(newStatus)
    
    // Listen for data (JSON responses)
    const handleData = (data: any) => {
        // Here we would process specific JSON responses
        // For example bandwidth stats updates
        if (data.type === 'BANDWIDTH_STATS') {
            setBandwidthStats(data.payload)
        }
    }

    window.api.on('daemon-status', handleStatus)
    window.api.on('daemon-log', handleLog)
    window.api.on('daemon-data', handleData)

    return () => {
      // Cleanup (if we implemented off in preload properly)
    }
  }, [])

  const sendCommand = async (cmd: string) => {
    const res = await window.api.sendCommand(cmd)
    if (!res.success) {
        handleLog(`ERROR: Failed to send command ${cmd}: ${res.error}`)
    }
  }

  const togglePause = () => {
    if (isPaused) {
        sendCommand('RESUME')
        setIsPaused(false)
    } else {
        sendCommand('PAUSE')
        setIsPaused(true)
    }
  }

  return (
    <div className="flex h-screen bg-background text-foreground font-sans overflow-hidden">
      {/* Sidebar */}
      <div className="w-64 border-r border-border bg-card flex flex-col">
        <div className="p-6 border-b border-border flex items-center gap-3">
          <Shield className="w-8 h-8 text-blue-500" />
          <div>
            <h1 className="font-bold text-lg tracking-tight">SentinelFS</h1>
            <p className="text-xs text-muted-foreground">Neo Core v1.0</p>
          </div>
        </div>

        <nav className="flex-1 p-4 space-y-2">
          <SidebarItem 
            icon={<Activity />} 
            label="Dashboard" 
            active={activeTab === 'dashboard'} 
            onClick={() => setActiveTab('dashboard')} 
          />
          <SidebarItem 
            icon={<Users />} 
            label="Network Mesh" 
            active={activeTab === 'peers'} 
            onClick={() => setActiveTab('peers')} 
          />
          <SidebarItem 
            icon={<Settings />} 
            label="Settings" 
            active={activeTab === 'settings'} 
            onClick={() => setActiveTab('settings')} 
          />
          <SidebarItem 
            icon={<Terminal />} 
            label="Debug Console" 
            active={activeTab === 'logs'} 
            onClick={() => setActiveTab('logs')} 
          />
        </nav>

        <div className="p-4 border-t border-border bg-secondary/30">
          <div className="flex items-center justify-between mb-2">
            <span className="text-xs font-medium">Daemon Status</span>
            <span className={`w-2 h-2 rounded-full ${
              status === 'connected' ? 'bg-green-500 shadow-[0_0_8px_rgba(34,197,94,0.6)]' : 
              status === 'error' ? 'bg-red-500' : 'bg-yellow-500'
            }`} />
          </div>
          <div className="text-xs text-muted-foreground truncate">
            {status === 'connected' ? 'Active & Monitoring' : 'Waiting for connection...'}
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div className="flex-1 flex flex-col overflow-hidden">
        <header className="h-16 border-b border-border flex items-center justify-between px-6 bg-card/50 backdrop-blur">
          <h2 className="font-semibold text-lg capitalize">{activeTab}</h2>
          
          <div className="flex items-center gap-3">
            <button 
                onClick={togglePause}
                className={`flex items-center gap-2 px-4 py-2 rounded-md text-sm font-medium transition-colors ${
                    isPaused 
                    ? 'bg-green-500/10 text-green-500 hover:bg-green-500/20 border border-green-500/20' 
                    : 'bg-yellow-500/10 text-yellow-500 hover:bg-yellow-500/20 border border-yellow-500/20'
                }`}
            >
              {isPaused ? <Play className="w-4 h-4" /> : <Pause className="w-4 h-4" />}
              {isPaused ? 'Resume Sync' : 'Pause Sync'}
            </button>
            <button 
                onClick={() => sendCommand('STATUS')}
                className="p-2 rounded-md hover:bg-secondary text-muted-foreground hover:text-foreground transition-colors"
            >
              <RefreshCw className="w-5 h-5" />
            </button>
          </div>
        </header>

        <main className="flex-1 overflow-auto p-6">
          {activeTab === 'dashboard' && <DashboardView />}
          {activeTab === 'peers' && <PeersView />}
          {activeTab === 'settings' && <SettingsView />}
          {activeTab === 'logs' && <LogsView logs={logs} />}
        </main>
      </div>
    </div>
  )
}

function SidebarItem({ icon, label, active, onClick }: any) {
  return (
    <button
      onClick={onClick}
      className={`w-full flex items-center gap-3 px-4 py-3 rounded-lg text-sm font-medium transition-all duration-200 ${
        active 
          ? 'bg-blue-600 text-white shadow-lg shadow-blue-900/20' 
          : 'text-muted-foreground hover:bg-secondary hover:text-foreground'
      }`}
    >
      {cloneElement(icon, { size: 18 })}
      {label}
    </button>
  )
}

function DashboardView() {
  return (
    <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
      <StatsCard 
        title="Active Peers" 
        value="0" 
        icon={<Users className="text-blue-500" />} 
        trend="+0 active"
      />
      <StatsCard 
        title="Sync Status" 
        value="Idle" 
        icon={<RefreshCw className="text-green-500" />} 
        trend="Up to date"
      />
      <StatsCard 
        title="Transfer Rate" 
        value="0 KB/s" 
        icon={<Wifi className="text-purple-500" />} 
        trend="0 KB downloaded"
      />
      
      <div className="col-span-full bg-card border border-border rounded-xl p-6 shadow-sm">
        <h3 className="font-semibold mb-4 flex items-center gap-2">
            <Activity className="w-5 h-5 text-blue-500" />
            Recent Activity
        </h3>
        <div className="space-y-4">
            <div className="text-sm text-muted-foreground text-center py-8">
                No recent file changes detected.
            </div>
        </div>
      </div>
    </div>
  )
}

function StatsCard({ title, value, icon, trend }: any) {
    return (
        <div className="bg-card border border-border rounded-xl p-6 shadow-sm hover:border-blue-500/30 transition-colors group">
            <div className="flex items-center justify-between mb-4">
                <h3 className="text-sm font-medium text-muted-foreground">{title}</h3>
                <div className="p-2 rounded-lg bg-secondary group-hover:bg-background transition-colors">
                    {icon}
                </div>
            </div>
            <div className="text-2xl font-bold mb-1">{value}</div>
            <div className="text-xs text-muted-foreground">{trend}</div>
        </div>
    )
}

function PeersView() {
    return (
        <div className="space-y-6">
            <div className="bg-card border border-border rounded-xl overflow-hidden">
                <div className="p-6 border-b border-border flex justify-between items-center">
                    <h3 className="font-semibold">Connected Devices</h3>
                    <button className="text-sm text-blue-500 hover:underline">Scan Network</button>
                </div>
                <div className="p-8 text-center text-muted-foreground">
                    <Radio className="w-12 h-12 mx-auto mb-4 opacity-20 animate-pulse" />
                    <p>Searching for SentinelFS peers on local network...</p>
                </div>
            </div>
        </div>
    )
}

function SettingsView() {
    return (
        <div className="max-w-2xl space-y-8">
            <div className="bg-card border border-border rounded-xl p-6 space-y-6">
                <h3 className="font-semibold flex items-center gap-2 border-b border-border pb-4">
                    <HardDrive className="w-5 h-5" />
                    Sync Configuration
                </h3>
                
                <div className="space-y-4">
                    <div>
                        <label className="block text-sm font-medium mb-1">Sync Directory</label>
                        <div className="flex gap-2">
                            <input type="text" readOnly value="~/SentinelFS" className="flex-1 bg-secondary border border-input rounded-md px-3 py-2 text-sm" />
                            <button className="px-4 py-2 bg-secondary hover:bg-secondary/80 rounded-md text-sm">Change</button>
                        </div>
                    </div>
                    
                    <div>
                        <label className="block text-sm font-medium mb-1">Session Code</label>
                        <div className="flex gap-2">
                            <input type="password" value="SENT-XXXX-XXXX" readOnly className="flex-1 bg-secondary border border-input rounded-md px-3 py-2 text-sm font-mono" />
                            <button className="px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-md text-sm">Reveal</button>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    )
}

function LogsView({ logs }: { logs: string[] }) {
    return (
        <div className="bg-black/90 rounded-lg border border-border font-mono text-xs p-4 h-full overflow-auto shadow-inner">
            {logs.length === 0 ? (
                <div className="text-muted-foreground italic opacity-50">No logs received yet...</div>
            ) : (
                logs.map((log, i) => (
                    <div key={i} className="mb-1 break-all hover:bg-white/5 px-1 rounded">
                        <span className="text-blue-500 opacity-50 mr-2">{new Date().toLocaleTimeString()}</span>
                        {log}
                    </div>
                ))
            )}
        </div>
    )
}
