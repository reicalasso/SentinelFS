import { X, Smartphone, FolderSync, Server, Radio, Folder, CheckCircle, PauseCircle } from 'lucide-react'
import { Section } from './Section'
import { Toggle } from './Toggle'

interface GeneralSettingsProps {
  config: any
  syncEnabled: boolean
  onSyncToggle: () => void
}

export function GeneralSettings({ config, syncEnabled, onSyncToggle }: GeneralSettingsProps) {
  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center gap-3 mb-2">
        <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-blue-500 to-cyan-600 flex items-center justify-center shadow-lg">
          <Smartphone className="w-5 h-5 text-white" />
        </div>
        <div>
          <h3 className="font-bold text-lg">General Settings</h3>
          <p className="text-xs text-muted-foreground">Core synchronization and configuration</p>
        </div>
      </div>

      {/* Sync Status Card */}
      <div className={`p-4 rounded-xl border-2 transition-all ${
        syncEnabled 
          ? 'border-emerald-500/50 bg-emerald-500/5' 
          : 'border-amber-500/50 bg-amber-500/5'
      }`}>
        <div className="flex items-center justify-between">
          <div className="flex items-center gap-3">
            {syncEnabled ? (
              <div className="w-10 h-10 rounded-full bg-emerald-500/20 flex items-center justify-center">
                <CheckCircle className="w-5 h-5 text-emerald-400" />
              </div>
            ) : (
              <div className="w-10 h-10 rounded-full bg-amber-500/20 flex items-center justify-center">
                <PauseCircle className="w-5 h-5 text-amber-400" />
              </div>
            )}
            <div>
              <span className="text-sm font-semibold block">
                {syncEnabled ? 'Synchronization Active' : 'Synchronization Paused'}
              </span>
              <span className="text-xs text-muted-foreground">
                {syncEnabled ? 'Files are being synced with peers' : 'File sync is temporarily disabled'}
              </span>
            </div>
          </div>
          <Toggle checked={syncEnabled} onChange={onSyncToggle} />
        </div>
      </div>
      
      {/* Network Configuration */}
      <Section title="Network Configuration">
        <div className="grid grid-cols-2 gap-3">
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
            <div className="flex items-center gap-2 text-muted-foreground mb-1">
              <Server className="w-4 h-4" />
              <span className="text-xs">TCP Port</span>
            </div>
            <div className="font-mono font-bold">{config?.tcpPort || 8080}</div>
          </div>
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
            <div className="flex items-center gap-2 text-muted-foreground mb-1">
              <Radio className="w-4 h-4" />
              <span className="text-xs">Discovery Port</span>
            </div>
            <div className="font-mono font-bold">{config?.discoveryPort || 9999}</div>
          </div>
        </div>
      </Section>

      {/* Watch Directory */}
      <Section title="Watch Directory">
        <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
          <div className="flex items-center gap-2 text-muted-foreground mb-1">
            <FolderSync className="w-4 h-4" />
            <span className="text-xs">Primary Watch Directory</span>
          </div>
          <div className="font-mono text-sm break-all">{config?.watchDirectory || '~/sentinel_sync'}</div>
        </div>
        
        {config?.watchedFolders && config.watchedFolders.length > 0 && (
          <div className="mt-4">
            <div className="text-xs text-muted-foreground mb-2 flex items-center gap-2">
              <Folder className="w-3 h-3" />
              Additional Watch Folders ({config.watchedFolders.length})
            </div>
            <div className="space-y-2">
              {config.watchedFolders.map((folder: string, i: number) => (
                <div key={i} className="flex justify-between items-center group p-2 rounded-lg bg-accent/20 hover:bg-accent/40 transition-colors">
                  <span className="font-mono text-xs break-all">{folder}</span>
                  <button 
                    onClick={async () => {
                      if (confirm(`Stop watching ${folder}?\n\nFiles will remain on disk, only monitoring will stop.`)) {
                        if (window.api) await window.api.sendCommand(`REMOVE_WATCH ${folder}`)
                      }
                    }}
                    className="opacity-0 group-hover:opacity-100 p-1.5 hover:bg-red-500/20 text-red-400 rounded transition-all"
                    title="Stop watching (files will NOT be deleted)"
                  >
                    <X className="w-3.5 h-3.5" />
                  </button>
                </div>
              ))}
            </div>
          </div>
        )}
      </Section>
    </div>
  )
}
