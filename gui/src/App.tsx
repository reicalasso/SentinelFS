import { useState, useEffect, cloneElement } from 'react'
import { Activity, Folder, HardDrive, Settings as SettingsIcon, Shield, Terminal, Users, Play, Pause, RefreshCw, ArrowRightLeft } from 'lucide-react'
import { Dashboard } from './components/Dashboard'
import { Peers } from './components/Peers'
import { Settings } from './components/Settings'
import { Files } from './components/Files'
import { Transfers } from './components/Transfers'

// Define the API interface exposed from preload
interface ElectronAPI {
  on: (channel: string, callback: (data: any) => void) => void
  off: (channel: string, callback: (data: any) => void) => void
  sendCommand: (command: string) => Promise<{ success: boolean; error?: string }>
  selectFolder: () => Promise<string | null>
}

declare global {
  interface Window {
    api: ElectronAPI
  }
}

type Tab = 'dashboard' | 'files' | 'peers' | 'transfers' | 'settings' | 'logs'

export default function App() {
  const [activeTab, setActiveTab] = useState<Tab>('dashboard')
  const [status, setStatus] = useState<'connected' | 'disconnected' | 'error'>('disconnected')
  const [logs, setLogs] = useState<string[]>([])
  const [isPaused, setIsPaused] = useState(false)
  
  // App State
  const [metrics, setMetrics] = useState<any>(null)
  const [peers, setPeers] = useState<any[]>([])
  const [syncStatus, setSyncStatus] = useState<any>(null)
  const [files, setFiles] = useState<any[]>([])
  const [activity, setActivity] = useState<any[]>([])
  const [transfers, setTransfers] = useState<any[]>([])
  const [config, setConfig] = useState<any>(null)
  const [lastPeerCount, setLastPeerCount] = useState<number>(0)

  const handleLog = (log: string) => {
    // Check if log has timestamp [YYYY-MM-DD HH:MM:SS]
    // Daemon sends ANSI colors sometimes, strip them for checking
    const cleanLog = log.replace(/\u001b\[[0-9;]*m/g, '')
    const hasTimestamp = /^\[\d{4}-\d{2}-\d{2}/.test(cleanLog)
    
    const formattedLog = hasTimestamp ? log : `[${new Date().toLocaleTimeString()}] ${log}`
    setLogs(prev => [...prev.slice(-99), formattedLog]) // Keep last 100 logs
  }

  useEffect(() => {
    if (window.api) {
        const handleStatus = (newStatus: any) => setStatus(newStatus)
        
        const handleData = (data: any) => {
            if (data.type === 'METRICS') {
                setMetrics(data.payload)
            } else if (data.type === 'PEERS') {
                // Ignore empty arrays - keep last known peer list to prevent flickering
                if (data.payload.length > 0) {
                    setPeers(data.payload)
                    setLastPeerCount(data.payload.length)
                } else if (lastPeerCount === 0) {
                    // Only set to empty if we never had peers
                    setPeers([])
                }
            } else if (data.type === 'STATUS') {
                setSyncStatus(data.payload)
            } else if (data.type === 'FILES') {
                setFiles(data.payload)
            } else if (data.type === 'ACTIVITY') {
                setActivity(data.payload)
            } else if (data.type === 'TRANSFERS') {
                setTransfers(data.payload)
            } else if (data.type === 'CONFIG') {
                setConfig(data.payload)
            }
        }

        window.api.on('daemon-status', handleStatus)
        window.api.on('daemon-log', handleLog)
        window.api.on('daemon-data', handleData)
        
        return () => {
            // Cleanup not strictly necessary as this effect runs once
        }
    }
  }, [])

  const sendCommand = async (cmd: string) => {
    if (window.api) {
        const res = await window.api.sendCommand(cmd)
        if (!res.success) {
            handleLog(`ERROR: Failed to send command ${cmd}: ${res.error}`)
        }
    } else {
        console.log(`[Mock Command] ${cmd}`)
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
    <div className="flex h-screen bg-background text-foreground font-sans overflow-hidden selection:bg-blue-500/30">
      {/* Sidebar */}
      <div className="w-64 border-r border-border bg-card/50 flex flex-col backdrop-blur-xl">
        <div className="p-6 flex items-center gap-3 select-none">
          <div className="relative">
            <Shield className="w-8 h-8 text-blue-500" />
            <div className="absolute inset-0 bg-blue-500 blur-lg opacity-20"></div>
          </div>
          <div>
            <h1 className="font-bold text-lg tracking-tight leading-none">SentinelFS</h1>
            <p className="text-[10px] text-muted-foreground font-medium tracking-wider uppercase mt-1">Neo Core v1.0</p>
          </div>
        </div>

        <nav className="flex-1 px-3 py-4 space-y-1">
          <SidebarItem 
            icon={<Activity />} 
            label="Dashboard" 
            active={activeTab === 'dashboard'} 
            onClick={() => setActiveTab('dashboard')} 
          />
          <SidebarItem 
            icon={<Folder />} 
            label="My Files" 
            active={activeTab === 'files'} 
            onClick={() => setActiveTab('files')} 
          />
          <SidebarItem 
            icon={<Users />} 
            label="Network Mesh" 
            active={activeTab === 'peers'} 
            onClick={() => setActiveTab('peers')} 
          />
          <SidebarItem 
            icon={<ArrowRightLeft />} 
            label="Transfers" 
            active={activeTab === 'transfers'} 
            onClick={() => setActiveTab('transfers')} 
          />
          <SidebarItem 
            icon={<SettingsIcon />} 
            label="Settings" 
            active={activeTab === 'settings'} 
            onClick={() => setActiveTab('settings')} 
          />
          
          <div className="pt-4 mt-4 border-t border-border/50">
            <SidebarItem 
                icon={<Terminal />} 
                label="Debug Console" 
                active={activeTab === 'logs'} 
                onClick={() => setActiveTab('logs')} 
            />
          </div>
        </nav>

        <div className="p-4 m-3 rounded-xl bg-secondary/30 border border-border/50 backdrop-blur-sm">
          <div className="flex items-center justify-between mb-2">
            <span className="text-[10px] font-bold uppercase tracking-wider text-muted-foreground">Daemon Status</span>
            <span className={`w-2 h-2 rounded-full transition-all duration-500 ${
              status === 'connected' ? 'bg-green-500 shadow-[0_0_8px_rgba(34,197,94,0.6)]' : 
              status === 'error' ? 'bg-red-500' : 'bg-yellow-500 animate-pulse'
            }`} />
          </div>
          <div className="text-xs font-medium truncate">
            {status === 'connected' ? 'Active & Monitoring' : 'Waiting for connection...'}
          </div>
        </div>
      </div>

      {/* Main Content */}
      <div className="flex-1 flex flex-col overflow-hidden bg-background/95">
        <header className="h-16 border-b border-border/50 flex items-center justify-between px-8 bg-background/50 backdrop-blur-sm sticky top-0 z-10">
          <div>
            <h2 className="font-bold text-lg capitalize tracking-tight">{activeTab.replace('-', ' ')}</h2>
          </div>
          
          <div className="flex items-center gap-3">
            <button 
                onClick={togglePause}
                className={`flex items-center gap-2 px-4 py-2 rounded-lg text-xs font-bold tracking-wide uppercase transition-all ${
                    isPaused 
                    ? 'bg-green-500/10 text-green-500 hover:bg-green-500/20 border border-green-500/20' 
                    : 'bg-yellow-500/10 text-yellow-500 hover:bg-yellow-500/20 border border-yellow-500/20'
                }`}
            >
              {isPaused ? <Play className="w-3.5 h-3.5" /> : <Pause className="w-3.5 h-3.5" />}
              {isPaused ? 'Resume Sync' : 'Pause Sync'}
            </button>
            <button 
                onClick={() => sendCommand('STATUS')}
                className="p-2 rounded-lg hover:bg-secondary text-muted-foreground hover:text-foreground transition-colors"
                title="Refresh Status"
            >
              <RefreshCw className="w-5 h-5" />
            </button>
          </div>
        </header>

        <main className="flex-1 overflow-auto p-8 scroll-smooth">
          <div className="max-w-6xl mx-auto">
            {activeTab === 'dashboard' && <Dashboard metrics={metrics} syncStatus={syncStatus} peersCount={peers.length} activity={activity} />}
            {activeTab === 'files' && <Files files={files} />}
            {activeTab === 'peers' && <Peers peers={peers} />}
            {activeTab === 'transfers' && <Transfers metrics={metrics} transfers={transfers} />}
            {activeTab === 'settings' && <Settings config={config} />}
            {activeTab === 'logs' && <LogsView logs={logs} />}
          </div>
        </main>
      </div>
    </div>
  )
}

function SidebarItem({ icon, label, active, onClick }: any) {
  return (
    <button
      onClick={onClick}
      className={`w-full flex items-center gap-3 px-4 py-2.5 rounded-lg text-sm font-medium transition-all duration-200 group ${
        active 
          ? 'bg-blue-600 text-white shadow-lg shadow-blue-900/20' 
          : 'text-muted-foreground hover:bg-secondary hover:text-foreground'
      }`}
    >
      {cloneElement(icon, { size: 18, className: `transition-transform group-hover:scale-110 ${active ? '' : 'opacity-70'}` })}
      {label}
    </button>
  )
}

function LogsView({ logs }: { logs: string[] }) {
    return (
        <div className="space-y-4 animate-in fade-in duration-500">
            <div className="flex items-center justify-between">
                <h2 className="text-lg font-semibold">Debug Console</h2>
                <button className="text-xs text-blue-500 hover:text-blue-400">Clear Logs</button>
            </div>
            <div className="bg-black/90 rounded-xl border border-border/50 font-mono text-xs p-4 h-[calc(100vh-200px)] overflow-auto shadow-inner">
                {logs.length === 0 ? (
                    <div className="h-full flex flex-col items-center justify-center text-muted-foreground opacity-50">
                        <Terminal className="w-8 h-8 mb-2" />
                        <p>No logs received yet...</p>
                    </div>
                ) : (
                    logs.map((log, i) => (
                        <div key={i} className="mb-1 break-all hover:bg-white/5 px-2 py-0.5 rounded transition-colors flex gap-2">
                            <span>{log}</span>
                        </div>
                    ))
                )}
            </div>
        </div>
    )
}
