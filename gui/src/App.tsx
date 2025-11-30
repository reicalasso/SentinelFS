import { useState, useEffect, cloneElement, useRef } from 'react'
import { Activity, Folder, Settings as SettingsIcon, Shield, Terminal, Users, Play, Pause, RefreshCw, ArrowRightLeft, Menu, Command } from 'lucide-react'
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

  const lastLogRef = useRef<string | null>(null)

  const handleLog = (log: string) => {
    const cleanLog = log.replace(/\u001b\[[0-9;]*m/g, '')
    const hasTimestamp = /^\[\d{4}-\d{2}-\d{2}/.test(cleanLog)
    const formattedLog = hasTimestamp ? log : `[${new Date().toLocaleTimeString()}] ${log}`
    if (lastLogRef.current === formattedLog) return
    lastLogRef.current = formattedLog
    setLogs(prev => [...prev.slice(-99), formattedLog])
  }

  useEffect(() => {
    if (!window.api) return

    const handleStatus = (newStatus: any) => setStatus(newStatus)
    
    const handleData = (data: any) => {
      if (data.type === 'METRICS') setMetrics(data.payload)
      else if (data.type === 'PEERS') {
        if (data.payload.length > 0) {
          setPeers(data.payload)
          setLastPeerCount(data.payload.length)
        } else if (lastPeerCount === 0) {
          setPeers([])
        }
      } else if (data.type === 'STATUS') setSyncStatus(data.payload)
      else if (data.type === 'FILES') setFiles(data.payload)
      else if (data.type === 'ACTIVITY') setActivity(data.payload)
      else if (data.type === 'TRANSFERS') setTransfers(data.payload)
      else if (data.type === 'CONFIG') setConfig(data.payload)
    }

    window.api.on('daemon-status', handleStatus)
    window.api.on('daemon-log', handleLog)
    window.api.on('daemon-data', handleData)

    // Clean up listeners to avoid duplicate logs (especially under React StrictMode)
    return () => {
      window.api.off('daemon-status', handleStatus)
      window.api.off('daemon-log', handleLog)
      window.api.off('daemon-data', handleData)
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

  const clearLogs = () => {
    setLogs([])
    lastLogRef.current = null
  }

  return (
    <div className="flex h-screen bg-background text-foreground font-sans overflow-hidden selection:bg-primary/30">
      {/* Modern Sidebar */}
      <div className="w-[280px] flex-shrink-0 border-r border-border/40 bg-card/30 flex flex-col backdrop-blur-2xl z-20">
        <div className="p-6 flex items-center gap-3 select-none">
          <div className="relative group cursor-pointer">
            <div className="absolute inset-0 bg-primary blur-xl opacity-20 group-hover:opacity-40 transition-opacity duration-500"></div>
            <div className="relative bg-primary/10 p-2.5 rounded-xl border border-primary/20 group-hover:border-primary/40 transition-colors">
                <Shield className="w-6 h-6 text-primary" />
            </div>
          </div>
          <div>
            <h1 className="font-bold text-lg tracking-tight leading-none flex items-center gap-2">
              SentinelFS
              <span className="px-1.5 py-0.5 rounded bg-primary/10 text-[10px] text-primary font-bold tracking-wide uppercase">Neo</span>
            </h1>
            <p className="text-[10px] text-muted-foreground font-medium tracking-wider mt-1">Distributed Sync Engine</p>
          </div>
        </div>

        <div className="flex-1 px-4 py-2 space-y-8 overflow-y-auto no-scrollbar">
          <div className="space-y-1">
            <h3 className="px-3 text-[10px] font-bold uppercase text-muted-foreground/50 tracking-wider mb-2">Main</h3>
            <SidebarItem icon={<Activity />} label="Dashboard" active={activeTab === 'dashboard'} onClick={() => setActiveTab('dashboard')} />
            <SidebarItem icon={<Folder />} label="SYNC Files" active={activeTab === 'files'} onClick={() => setActiveTab('files')} />
            <SidebarItem icon={<Users />} label="Network Mesh" active={activeTab === 'peers'} onClick={() => setActiveTab('peers')} badge={peers.length > 0 ? peers.length : undefined} />
            <SidebarItem icon={<ArrowRightLeft />} label="Transfers" active={activeTab === 'transfers'} onClick={() => setActiveTab('transfers')} />
          </div>

          <div className="space-y-1">
             <h3 className="px-3 text-[10px] font-bold uppercase text-muted-foreground/50 tracking-wider mb-2">System</h3>
            <SidebarItem icon={<SettingsIcon />} label="Settings" active={activeTab === 'settings'} onClick={() => setActiveTab('settings')} />
            <SidebarItem icon={<Terminal />} label="Debug Console" active={activeTab === 'logs'} onClick={() => setActiveTab('logs')} />
          </div>
        </div>

        {/* Daemon Status Footer */}
        <div className="p-4 mt-auto border-t border-border/40 bg-background/20 backdrop-blur-sm">
           <div className="flex items-center justify-between gap-3 bg-secondary/50 p-3 rounded-xl border border-border/50 hover:border-primary/30 transition-colors cursor-default">
              <div className="flex items-center gap-3">
                <div className={`w-2.5 h-2.5 rounded-full transition-all duration-500 ${
                  status === 'connected' ? 'bg-emerald-500 shadow-[0_0_8px_rgba(16,185,129,0.5)]' : 
                  status === 'error' ? 'bg-red-500' : 'bg-amber-500 animate-pulse'
                }`} />
                <div className="flex flex-col">
                    <span className="text-xs font-semibold">Daemon Status</span>
                    <span className="text-[10px] text-muted-foreground truncate max-w-[120px]">
                        {status === 'connected' ? 'Active & Monitoring' : 'Waiting for connection...'}
                    </span>
                </div>
              </div>
              <button onClick={() => sendCommand('STATUS')} className="p-1.5 hover:bg-background rounded-lg text-muted-foreground hover:text-foreground transition-colors">
                <RefreshCw className="w-3.5 h-3.5" />
              </button>
           </div>
        </div>
      </div>

      {/* Main Content Area */}
      <div className="flex-1 flex flex-col min-w-0 relative">
        {/* Glass Header */}
        <header className="h-16 px-8 flex items-center justify-between sticky top-0 z-10 backdrop-blur-md bg-background/60 border-b border-border/40 transition-all">
          <div className="flex items-center gap-4">
             {/* Breadcrumbs or Title */}
             <div className="flex items-center gap-2 text-sm text-muted-foreground">
                <Command className="w-4 h-4 opacity-50" />
                <span>/</span>
                <span className="font-semibold text-foreground capitalize">{activeTab.replace('-', ' ')}</span>
             </div>
          </div>
          
          <div className="flex items-center gap-3">
            <div className="hidden md:flex items-center gap-2 px-3 py-1.5 bg-secondary/50 rounded-full border border-border/50 text-xs text-muted-foreground font-mono">
               <div className={`w-1.5 h-1.5 rounded-full ${isPaused ? 'bg-amber-500' : 'bg-emerald-500'}`}></div>
               {isPaused ? 'SYNC PAUSED' : 'SYNC ACTIVE'}
            </div>

            <button 
                onClick={togglePause}
                className={`flex items-center gap-2 px-4 py-2 rounded-lg text-xs font-bold tracking-wide uppercase transition-all shadow-sm hover:shadow-md active:scale-95 ${
                    isPaused 
                    ? 'bg-emerald-500/10 text-emerald-600 hover:bg-emerald-500/20 border border-emerald-500/20' 
                    : 'bg-amber-500/10 text-amber-600 hover:bg-amber-500/20 border border-amber-500/20'
                }`}
            >
              {isPaused ? <Play className="w-3.5 h-3.5 fill-current" /> : <Pause className="w-3.5 h-3.5 fill-current" />}
              {isPaused ? 'Resume' : 'Pause'}
            </button>
          </div>
        </header>

        <main className="flex-1 overflow-y-auto p-6 md:p-8 scroll-smooth">
          <div className="max-w-7xl mx-auto">
            {activeTab === 'dashboard' && <Dashboard metrics={metrics} syncStatus={syncStatus} peersCount={peers.length} activity={activity} />}
            {activeTab === 'files' && <Files files={files} />}
            {activeTab === 'peers' && <Peers peers={peers} />}
            {activeTab === 'transfers' && <Transfers metrics={metrics} transfers={transfers} />}
            {activeTab === 'settings' && <Settings config={config} />}
            {activeTab === 'logs' && <LogsView logs={logs} onClear={clearLogs} />}
          </div>
        </main>
      </div>
    </div>
  )
}

function SidebarItem({ icon, label, active, onClick, badge }: any) {
  return (
    <button
      onClick={onClick}
      className={`w-full flex items-center gap-3 px-4 py-2.5 rounded-xl text-sm font-medium transition-all duration-200 group relative overflow-hidden ${
        active 
          ? 'bg-primary text-primary-foreground shadow-lg shadow-primary/20' 
          : 'text-muted-foreground hover:bg-secondary/80 hover:text-foreground'
      }`}
    >
      {/* Active Indicator Line */}
      {active && <div className="absolute left-0 top-1/2 -translate-y-1/2 w-1 h-5 bg-white/20 rounded-r-full"></div>}
      
      {cloneElement(icon, { size: 18, className: `transition-transform group-hover:scale-110 ${active ? '' : 'opacity-70 group-hover:opacity-100'}` })}
      <span className="flex-1 text-left">{label}</span>
      
      {badge !== undefined && (
        <span className={`text-[10px] px-1.5 py-0.5 rounded-md font-bold ${
            active ? 'bg-white/20 text-white' : 'bg-secondary-foreground/10 text-secondary-foreground'
        }`}>
            {badge}
        </span>
      )}
    </button>
  )
}

function LogsView({ logs, onClear }: { logs: string[]; onClear: () => void }) {
    return (
        <div className="space-y-4 animate-in fade-in duration-500 slide-in-from-bottom-4">
            <div className="flex items-center justify-between">
                <div>
                    <h2 className="text-lg font-semibold flex items-center gap-2">
                        <Terminal className="w-5 h-5 text-muted-foreground" />
                        Debug Console
                    </h2>
                    <p className="text-sm text-muted-foreground">Real-time daemon logs and events</p>
                </div>
                <button
                    onClick={onClear}
                    className="text-xs px-3 py-1.5 rounded-lg bg-secondary hover:bg-destructive/10 hover:text-destructive transition-colors font-medium"
                >
                    Clear Logs
                </button>
            </div>
            <div className="bg-black/80 backdrop-blur-md rounded-xl border border-border/50 font-mono text-xs p-4 h-[calc(100vh-220px)] overflow-auto shadow-inner selection:bg-primary/30">
                {logs.length === 0 ? (
                    <div className="h-full flex flex-col items-center justify-center text-muted-foreground/40">
                        <Terminal className="w-12 h-12 mb-3 opacity-50" />
                        <p>Waiting for daemon logs...</p>
                    </div>
                ) : (
                    logs.map((log, i) => (
                        <div key={i} className="mb-1.5 break-all hover:bg-white/5 px-2 py-1 rounded transition-colors flex gap-3 border-l-2 border-transparent hover:border-primary/50">
                            <span className="text-muted-foreground/50 select-none w-6 text-right">{i+1}</span>
                            <span className="text-zinc-300">{log}</span>
                        </div>
                    ))
                )}
            </div>
        </div>
    )
}
