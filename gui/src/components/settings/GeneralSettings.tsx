import { X } from 'lucide-react'
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
      <Section title="Synchronization">
        <div className="flex items-center justify-between">
          <div>
            <span className="text-sm font-medium block">Enable Sync</span>
            <span className="text-xs text-muted-foreground">Pause/resume file synchronization</span>
          </div>
          <Toggle checked={syncEnabled} onChange={onSyncToggle} />
        </div>
      </Section>
      
      <Section title="Configuration">
        <div className="space-y-2 text-xs">
          <div className="flex justify-between py-1">
            <span className="text-muted-foreground">TCP Port:</span>
            <span className="font-mono">{config?.tcpPort || 8080}</span>
          </div>
          <div className="flex justify-between py-1">
            <span className="text-muted-foreground">Discovery Port:</span>
            <span className="font-mono">{config?.discoveryPort || 9999}</span>
          </div>
          <div className="flex justify-between py-1">
            <span className="text-muted-foreground">Watch Directory:</span>
            <span className="font-mono text-xs">{config?.watchDirectory || '~/sentinel_sync'}</span>
          </div>
          
          {config?.watchedFolders && config.watchedFolders.length > 0 && (
            <div className="pt-2 mt-2 border-t border-border/50">
              <span className="text-muted-foreground block mb-1">Additional Watch Folders:</span>
              <div className="space-y-1 pl-1">
                {config.watchedFolders.map((folder: string, i: number) => (
                  <div key={i} className="flex justify-between items-center group">
                    <span className="font-mono text-xs break-all">{folder}</span>
                    <button 
                      onClick={async () => {
                        if (confirm(`Stop watching ${folder}?\n\nFiles will remain on disk, only monitoring will stop.`)) {
                          if (window.api) await window.api.sendCommand(`REMOVE_WATCH ${folder}`)
                        }
                      }}
                      className="opacity-0 group-hover:opacity-100 p-1 hover:bg-error-muted status-error rounded transition-opacity"
                      title="Stop watching (files will NOT be deleted)"
                    >
                      <X className="w-3 h-3" />
                    </button>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>
      </Section>
    </div>
  )
}
