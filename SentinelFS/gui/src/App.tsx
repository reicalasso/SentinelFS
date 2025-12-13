import { useEffect, useState, cloneElement } from 'react'
import { Activity, Folder, Settings as SettingsIcon, Shield, Terminal, Users, Play, Pause, RefreshCw, ArrowRightLeft, Command, AlertTriangle, Menu, X, ShieldAlert, Database } from 'lucide-react'
import { Dashboard } from './components/Dashboard'
import { Peers } from './components/Peers'
import { Settings } from './components/Settings'
import { Files } from './components/Files'
import { Transfers } from './components/Transfers'
import { ToastList } from './components/ToastList'
import { ConflictCenter } from './components/ConflictCenter'
import { QuarantineCenter } from './components/QuarantineCenter'
import { OnboardingWizard } from './components/OnboardingWizard'
import { FalconStore } from './components/FalconStore'
import { Logger } from './components/Logger'
import { mapDaemonError } from './errorMessages'
import { useAppState } from './hooks/useAppState'

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

export default function App() {
  // Use centralized state management
  const { state, actions } = useAppState()
  const { activeTab, status, logs, isPaused, metrics, peers, syncStatus, files, activity, transfers, transferHistory, config, toasts, conflicts, showConflictModal: isConflictModalOpen, versionedFiles, threatStatus, detectedThreats, showQuarantineModal: isQuarantineModalOpen } = state
  const { setTab, setStatus, setPaused, handleData, handleLog, clearLogs, addToast, clearToasts, showConflictModal, resolveConflict, deleteVersion, showQuarantineModal, removeThreat, markThreatSafe } = actions
  const [showOnboarding, setShowOnboarding] = useState(true)
  const [sidebarOpen, setSidebarOpen] = useState(false)

  useEffect(() => {
    if (!window.api) return

    const handleStatus = (newStatus: any) => setStatus(newStatus)

    window.api.on('daemon-status', handleStatus)
    window.api.on('daemon-log', handleLog)
    window.api.on('daemon-data', handleData)

    return () => {
      window.api.off('daemon-status', handleStatus)
      window.api.off('daemon-log', handleLog)
      window.api.off('daemon-data', handleData)
    }
  }, [setStatus, handleLog, handleData])

  const sendCommand = async (cmd: string) => {
    if (window.api) {
        const res = await window.api.sendCommand(cmd)
        if (!res.success) {
        const friendly = mapDaemonError(res.error)
        const logMessage = `ERROR: ${cmd} -> ${res.error ?? 'unknown'}`
        handleLog(logMessage)
        addToast(`${friendly.title}: ${friendly.message}`)
        }
    } else {
        console.log(`[Mock Command] ${cmd}`)
    }
  }

  const togglePause = () => {
    if (isPaused) {
        sendCommand('RESUME')
        setPaused(false)
    } else {
        sendCommand('PAUSE')
        setPaused(true)
    }
  }

  return (
    <div className="flex h-screen bg-background text-foreground font-sans overflow-hidden selection:bg-primary/30">
      {/* Mobile Overlay */}
      {sidebarOpen && (
        <div 
          className="fixed inset-0 bg-black/50 z-30 lg:hidden"
          onClick={() => setSidebarOpen(false)}
        />
      )}
      
      {/* Ultra Modern Sidebar */}
      <div className={`
        fixed lg:relative inset-y-0 left-0 z-40
        w-[280px] sm:w-[300px] flex-shrink-0 
        border-r border-border/30 bg-gradient-to-b from-card/80 to-card/40 
        flex flex-col backdrop-blur-2xl overflow-hidden
        transform transition-transform duration-300 ease-in-out
        ${sidebarOpen ? 'translate-x-0' : '-translate-x-full lg:translate-x-0'}
      `}>
        {/* Animated background gradient */}
        <div className="absolute inset-0 bg-gradient-to-br from-primary/5 via-transparent to-accent/5 pointer-events-none"></div>
        <div className="absolute top-0 left-0 w-full h-64 bg-gradient-to-b from-primary/10 to-transparent pointer-events-none"></div>
        
        {/* Logo Section */}
        <div className="p-4 sm:p-6 flex items-center gap-3 sm:gap-4 select-none relative">
          {/* Close button for mobile */}
          <button 
            className="lg:hidden absolute right-4 top-4 p-2 rounded-xl hover:bg-secondary/50 transition-colors"
            onClick={() => setSidebarOpen(false)}
          >
            <X className="w-5 h-5 text-muted-foreground" />
          </button>
          
          <div className="relative group cursor-pointer">
            {/* Subtle glow effect */}
            <div className="absolute inset-0 bg-primary/40 blur-xl opacity-0 group-hover:opacity-30 transition-opacity duration-500"></div>
            <div className="relative bg-gradient-to-br from-primary/15 to-primary/5 p-3 rounded-2xl border border-primary/20 group-hover:border-primary/40 transition-all duration-300 shadow-md group-hover:scale-105">
                <Shield className="w-7 h-7 text-primary" />
            </div>
          </div>
          <div>
            <h1 className="font-bold text-xl tracking-tight leading-none flex items-center gap-2">
              <span className="bg-gradient-to-r from-foreground to-foreground/70 bg-clip-text text-transparent">SentinelFS</span>
              <span className="px-2 py-0.5 rounded-lg bg-gradient-to-r from-primary/20 to-accent/20 text-[10px] text-primary font-bold tracking-widest uppercase border border-primary/20">Neo</span>
            </h1>
            <p className="text-[11px] text-muted-foreground font-medium tracking-wider mt-1.5 flex items-center gap-2">
              <span className="dot-success animate-pulse"></span>
              Distributed Sync Engine
            </p>
          </div>
        </div>

        <div className="flex-1 px-4 py-2 space-y-6 overflow-y-auto no-scrollbar relative">
          {/* Navigation Groups */}
          <div className="space-y-1">
            <h3 className="px-3 text-[10px] font-bold uppercase text-primary/60 tracking-[0.2em] mb-3 flex items-center gap-2">
              <div className="w-4 h-px bg-gradient-to-r from-primary/50 to-transparent"></div>
              Main
            </h3>
            <SidebarItem icon={<Activity />} label="Dashboard" active={activeTab === 'dashboard'} onClick={() => { setTab('dashboard'); setSidebarOpen(false); }} />
            <SidebarItem icon={<Folder />} label="SYNC Files" active={activeTab === 'files'} onClick={() => { setTab('files'); setSidebarOpen(false); }} />
            <SidebarItem icon={<Users />} label="NetFalcon" active={activeTab === 'peers'} onClick={() => { setTab('peers'); setSidebarOpen(false); }} badge={peers.length > 0 ? peers.length : undefined} />
            <SidebarItem icon={<ArrowRightLeft />} label="Transfers" active={activeTab === 'transfers'} onClick={() => { setTab('transfers'); setSidebarOpen(false); }} />
            <SidebarItem icon={<Database />} label="FalconStore" active={activeTab === 'falconstore'} onClick={() => { setTab('falconstore'); setSidebarOpen(false); }} />
          </div>

          <div className="space-y-1">
             <h3 className="px-3 text-[10px] font-bold uppercase text-accent/60 tracking-[0.2em] mb-3 flex items-center gap-2">
               <div className="w-4 h-px bg-gradient-to-r from-accent/50 to-transparent"></div>
               System
             </h3>
            <SidebarItem 
              icon={<AlertTriangle />} 
              label="Conflict Center" 
              active={false} 
              onClick={() => { showConflictModal(true); setSidebarOpen(false); }} 
              badge={conflicts.length > 0 ? conflicts.length : undefined}
              highlight={conflicts.length > 0}
            />
            <SidebarItem 
              icon={<ShieldAlert />} 
              label="Threat Quarantine" 
              active={false} 
              onClick={() => { showQuarantineModal(true); setSidebarOpen(false); }} 
              badge={detectedThreats.filter(t => !t.markedSafe).length > 0 ? detectedThreats.filter(t => !t.markedSafe).length : undefined}
              highlight={detectedThreats.filter(t => !t.markedSafe).length > 0}
            />
            <SidebarItem icon={<SettingsIcon />} label="Settings" active={activeTab === 'settings'} onClick={() => { setTab('settings'); setSidebarOpen(false); }} />
            <SidebarItem icon={<Terminal />} label="Debug Console" active={activeTab === 'logs'} onClick={() => { setTab('logs'); setSidebarOpen(false); }} />
          </div>
        </div>

        {/* Daemon Status Footer - Enhanced */}
        <div className="p-4 mt-auto border-t border-border/30 bg-gradient-to-t from-background/40 to-transparent backdrop-blur-sm relative">
          <div className="absolute inset-x-0 top-0 h-px bg-gradient-to-r from-transparent via-primary/30 to-transparent"></div>
          
           <div className="relative overflow-hidden flex items-center justify-between gap-3 bg-gradient-to-r from-secondary/60 to-secondary/30 p-4 rounded-2xl border border-border/50 hover:border-primary/30 transition-all cursor-default group shadow-lg">
              {/* Animated border glow */}
              <div className="absolute inset-0 rounded-2xl bg-gradient-to-r from-primary/0 via-primary/10 to-primary/0 opacity-0 group-hover:opacity-100 transition-opacity duration-500"></div>
              
              <div className="relative flex items-center gap-3">
                <div className="relative">
                  <div className={`w-3 h-3 rounded-full transition-all duration-500 ${
                    status === 'connected' 
                      ? 'bg-success glow-success' 
                      : status === 'error' 
                        ? 'bg-error glow-error' 
                        : 'bg-warning animate-pulse glow-warning'
                  }`} />
                  {status === 'connected' && (
                    <div className="absolute inset-0 rounded-full bg-success animate-ping opacity-30"></div>
                  )}
                </div>
                <div className="flex flex-col">
                    <span className="text-sm font-semibold">Daemon Status</span>
                    <span className="text-[11px] text-muted-foreground truncate max-w-[140px]">
                        {status === 'connected' ? 'Active & Monitoring' : 'Waiting for connection...'}
                    </span>
                </div>
              </div>
              <button onClick={() => sendCommand('STATUS')} className="relative p-2 hover:bg-background/50 rounded-xl text-muted-foreground hover:text-primary transition-all hover:scale-110 active:scale-95">
                <RefreshCw className="w-4 h-4" />
              </button>
           </div>
           
           {/* Acknowledgement */}
           <div className="mt-3 text-center">
             <p className="text-[10px] text-muted-foreground/50 italic">
               lifalif aldigi icin oralete'ye tesekkurler 💜
             </p>
           </div>
        </div>
      </div>

      <ToastList toasts={toasts} onClear={clearToasts} />

      {/* İlk açılış için onboarding sihirbazı */}
      <OnboardingWizard
        open={showOnboarding}
        onClose={() => setShowOnboarding(false)}
        onCompleted={() => setShowOnboarding(false)}
      />
      
      {/* Conflict Center - Enhanced conflict resolution and version history */}
      <ConflictCenter
        conflicts={conflicts.map(c => ({
          ...c,
          localHash: c.localHash || '',
          remoteHash: c.remoteHash || '',
          localTimestamp: typeof c.localTimestamp === 'string' ? new Date(c.localTimestamp).getTime() : c.localTimestamp,
          remoteTimestamp: typeof c.remoteTimestamp === 'string' ? new Date(c.remoteTimestamp).getTime() : c.remoteTimestamp,
          detectedAt: c.detectedAt || Date.now(),
          resolved: c.resolved || false
        }))}
        isOpen={isConflictModalOpen}
        onClose={() => showConflictModal(false)}
        onResolve={async (conflictId, resolution) => {
          if (window.api) {
            const cmd = `RESOLVE ${conflictId} ${resolution.toUpperCase()}`
            const res = await window.api.sendCommand(cmd)
            if (res.success) {
              resolveConflict(conflictId)
              addToast(`Conflict resolved: ${resolution}`)
            } else {
              addToast(`Failed to resolve conflict: ${res.error}`)
            }
          }
        }}
        onRestoreVersion={async (conflictId, versionId) => {
          if (window.api) {
            const cmd = `RESTORE_VERSION ${conflictId} ${versionId}`
            const res = await window.api.sendCommand(cmd)
            if (res.success) {
              addToast(`Version ${versionId} restored successfully`)
            } else {
              addToast(`Failed to restore version: ${res.error}`)
            }
          }
        }}
        onPreviewVersion={async (filePath, versionId) => {
          if (window.api) {
            const cmd = `PREVIEW_VERSION ${versionId} ${filePath}`
            await window.api.sendCommand(cmd)
          }
        }}
        onDeleteVersion={async (filePath, versionId) => {
          if (window.api) {
            const cmd = `DELETE_VERSION ${versionId} ${filePath}`
            const res = await window.api.sendCommand(cmd)
            if (res.success) {
              deleteVersion(filePath, versionId)
              addToast(`Version deleted`)
            } else {
              addToast(`Failed to delete version: ${res.error}`)
            }
          }
        }}
        versionedFiles={versionedFiles}
      />

      {/* Quarantine Center - Threat detection and management */}
      <QuarantineCenter
        threats={detectedThreats}
        isOpen={isQuarantineModalOpen}
        onClose={() => showQuarantineModal(false)}
        onDeleteThreat={async (threatId) => {
          if (window.api) {
            const cmd = `DELETE_THREAT ${threatId}`
            const res = await window.api.sendCommand(cmd)
            if (res.success) {
              addToast('Threat deleted successfully')
              // Refresh threat list and dashboard metrics from backend
              await window.api.sendCommand('THREATS_JSON')
              await window.api.sendCommand('THREAT_STATUS_JSON')
            } else {
              addToast(`Failed to delete threat: ${res.error}`)
            }
          }
        }}
        onMarkSafe={async (threatId) => {
          if (window.api) {
            const threat = detectedThreats.find(t => t.id === threatId)
            const isCurrentlySafe = threat?.markedSafe
            const cmd = isCurrentlySafe ? `UNMARK_THREAT_SAFE ${threatId}` : `MARK_THREAT_SAFE ${threatId}`
            const res = await window.api.sendCommand(cmd)
            if (res.success) {
              addToast(isCurrentlySafe ? 'Unmarked as safe' : 'Marked as safe')
              // Refresh threat list and dashboard metrics from backend
              await window.api.sendCommand('THREATS_JSON')
              await window.api.sendCommand('THREAT_STATUS_JSON')
            } else {
              addToast(`Operation failed: ${res.error}`)
            }
          }
        }}
      />
      
      {/* Main Content Area */}
      <div className="flex-1 flex flex-col min-w-0 relative">
        {/* Animated Background Pattern */}
        <div className="absolute inset-0 overflow-hidden pointer-events-none">
          <div className="absolute top-0 right-0 w-96 h-96 bg-primary/5 rounded-full blur-3xl"></div>
          <div className="absolute bottom-0 left-0 w-96 h-96 bg-accent/5 rounded-full blur-3xl"></div>
        </div>
        
        {/* Header - Clean & Minimal */}
        <header className="h-14 px-3 sm:px-6 flex items-center justify-between sticky top-0 z-10 backdrop-blur-md bg-background/70 border-b border-border/20">
          <div className="flex items-center gap-2 sm:gap-3">
             {/* Mobile Menu Button */}
             <button 
               className="lg:hidden p-2 rounded-xl hover:bg-secondary/50 transition-colors"
               onClick={() => setSidebarOpen(true)}
             >
               <Menu className="w-5 h-5" />
             </button>
             
             {/* Page Title - Simple */}
             <div className="flex items-center gap-2 text-sm">
                <Command className="w-4 h-4 text-muted-foreground hidden sm:block" />
                <span className="text-muted-foreground/60 hidden sm:inline">/</span>
                <span className="font-medium text-foreground capitalize">{activeTab.replace('-', ' ')}</span>
             </div>
          </div>
          
          <div className="flex items-center gap-2 sm:gap-3">
            {/* Sync Status Indicator - Minimal */}
            <div className="hidden sm:flex items-center gap-2 px-3 py-1.5 bg-secondary/40 rounded-lg border border-border/30 text-xs">
               <div className={`w-2 h-2 rounded-full ${isPaused ? 'bg-info' : 'bg-success'}`}></div>
               <span className={`font-medium ${isPaused ? 'status-info' : 'status-success'}`}>
                 {isPaused ? 'Paused' : 'Active'}
               </span>
            </div>

            {/* Toggle Button - Clean & Minimal */}
            <button 
                onClick={togglePause}
                className={`flex items-center gap-1 sm:gap-2 px-2 sm:px-3 py-1.5 rounded-lg text-xs font-medium transition-all active:scale-95 border ${
                    isPaused 
                    ? 'bg-success-muted status-success border-success/20 hover:bg-success/20 hover:border-success/30' 
                    : 'bg-info-muted status-info border-info/20 hover:bg-info/20 hover:border-info/30'
                }`}
            >
              {isPaused ? <Play className="w-3.5 h-3.5" /> : <Pause className="w-3.5 h-3.5" />}
              <span className="hidden sm:inline">{isPaused ? 'Resume' : 'Pause'}</span>
            </button>
          </div>
        </header>

        <main className="relative flex-1 overflow-y-auto p-4 sm:p-6 lg:p-8 scroll-smooth">
          <div className="max-w-7xl mx-auto">
            {activeTab === 'dashboard' && <Dashboard metrics={metrics as any} syncStatus={syncStatus} peersCount={peers.length} activity={activity as any} threatStatus={threatStatus as any} onOpenQuarantine={() => showQuarantineModal(true)} />}
            {activeTab === 'files' && <Files files={files} />}
            {activeTab === 'peers' && <Peers peers={peers} />}
            {activeTab === 'transfers' && <Transfers metrics={metrics as any} transfers={transfers} history={transferHistory} activity={activity as any} />}
            {activeTab === 'falconstore' && <FalconStore />}
            {activeTab === 'settings' && <Settings config={config} />}
            {activeTab === 'logs' && <Logger logs={logs} onClear={clearLogs} />}
          </div>
        </main>
      </div>
    </div>
  )
}

function SidebarItem({ icon, label, active, onClick, badge, highlight }: any) {
  return (
    <button
      onClick={onClick}
      className={`w-full flex items-center gap-3 px-4 py-3 rounded-xl text-sm font-medium transition-all duration-300 group relative overflow-hidden ${
        active 
          ? 'bg-gradient-to-r from-primary to-primary/80 text-primary-foreground shadow-lg shadow-primary/30' 
          : highlight 
            ? 'text-yellow-400 hover:bg-yellow-500/10 border border-yellow-500/30 shadow-sm shadow-yellow-500/10'
            : 'text-muted-foreground hover:bg-secondary/80 hover:text-foreground'
      }`}
    >
      {/* Animated Background Shine */}
      {active && (
        <div className="absolute inset-0 bg-gradient-to-r from-transparent via-white/10 to-transparent translate-x-[-100%] group-hover:translate-x-[100%] transition-transform duration-700"></div>
      )}
      
      {/* Highlight pulse animation for conflicts */}
      {highlight && !active && (
        <div className="absolute inset-0 bg-yellow-500/5 animate-pulse"></div>
      )}
      
      {/* Active Indicator - Enhanced */}
      {active && (
        <>
          <div className="absolute left-0 top-1/2 -translate-y-1/2 w-1 h-8 bg-white/30 rounded-r-full shadow-lg shadow-white/20"></div>
          <div className="absolute right-2 top-1/2 -translate-y-1/2 w-1.5 h-1.5 bg-white/50 rounded-full animate-pulse"></div>
        </>
      )}
      
      {/* Hover glow effect */}
      {!active && !highlight && (
        <div className="absolute inset-0 bg-gradient-to-r from-primary/0 via-primary/5 to-primary/0 opacity-0 group-hover:opacity-100 transition-opacity duration-300"></div>
      )}
      
      <div className={`relative transition-all duration-300 ${active ? '' : highlight ? 'text-yellow-400' : 'group-hover:scale-110 group-hover:text-primary'}`}>
        {cloneElement(icon, { size: 20, className: `transition-all ${active ? 'drop-shadow-lg' : highlight ? 'opacity-100' : 'opacity-70 group-hover:opacity-100'}` })}
      </div>
      <span className="flex-1 text-left relative">{label}</span>
      
      {badge !== undefined && (
        <span className={`relative text-[10px] px-2 py-0.5 rounded-lg font-bold transition-all ${
            active 
              ? 'bg-white/20 text-white shadow-inner' 
              : highlight
                ? 'bg-yellow-500/20 text-yellow-400 border border-yellow-500/30 animate-pulse'
                : 'bg-primary/10 text-primary border border-primary/20 group-hover:bg-primary/20'
        }`}>
            {badge}
        </span>
      )}
    </button>
  )
}

