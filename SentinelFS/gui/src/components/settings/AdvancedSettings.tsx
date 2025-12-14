import { Database, Download, Upload, Plus, X, FileX, Settings2, Cpu, AlertTriangle, RefreshCw, RotateCcw } from 'lucide-react'
import { Section } from './Section'

interface AdvancedSettingsProps {
  config: any
  ignorePatterns: string[]
  newPattern: string
  onNewPatternChange: (value: string) => void
  onAddPattern: () => void
  onRemovePattern: (pattern: string) => void
  onExportConfig: () => void
  onImportConfig: () => void
  onExportSupportBundle: () => void
  onRefreshStatus: () => void
  onResetDefaults: () => void
}

export function AdvancedSettings({
  config,
  ignorePatterns,
  newPattern,
  onNewPatternChange,
  onAddPattern,
  onRemovePattern,
  onExportConfig,
  onImportConfig,
  onExportSupportBundle,
  onRefreshStatus,
  onResetDefaults
}: AdvancedSettingsProps) {
  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center gap-3 mb-2">
        <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-slate-500 to-zinc-600 flex items-center justify-center shadow-lg">
          <Settings2 className="w-5 h-5 text-white" />
        </div>
        <div>
          <h3 className="font-bold text-lg">Advanced Settings</h3>
          <p className="text-xs text-muted-foreground">System configuration and maintenance</p>
        </div>
      </div>

      {/* System Information */}
      <Section title="System Information">
        <div className="grid grid-cols-1 sm:grid-cols-3 gap-3">
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
            <div className="flex items-center gap-2 text-muted-foreground mb-1">
              <Database className="w-4 h-4" />
              <span className="text-xs">Database</span>
            </div>
            <div className="font-mono text-sm">sentinel.db</div>
            <div className="text-xs text-muted-foreground">SQLite</div>
          </div>
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
            <div className="flex items-center gap-2 text-muted-foreground mb-1">
              <Cpu className="w-4 h-4" />
              <span className="text-xs">Metrics Port</span>
            </div>
            <div className="font-mono font-bold">{config?.metricsPort || 9100}</div>
          </div>
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50 sm:col-span-1">
            <div className="flex items-center gap-2 text-muted-foreground mb-1">
              <Settings2 className="w-4 h-4" />
              <span className="text-xs">IPC Socket</span>
            </div>
            <div className="font-mono text-xs break-all">/run/user/1000/sentinelfs/sentinel_daemon.sock</div>
          </div>
        </div>
      </Section>
      
      {/* Delta Sync Engine */}
      <Section title="Delta Sync Engine">
        <div className="grid grid-cols-2 gap-3">
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
            <div className="text-xs text-muted-foreground mb-1">Algorithm</div>
            <div className="font-mono text-sm">Rolling Checksum</div>
            <div className="text-xs text-muted-foreground">Adler32 + SHA-256</div>
          </div>
          <div className="p-3 rounded-lg bg-accent/30 border border-border/50">
            <div className="text-xs text-muted-foreground mb-1">Block Size</div>
            <div className="font-mono font-bold">4 KB</div>
            <div className="text-xs text-muted-foreground">Adaptive sizing</div>
          </div>
        </div>
      </Section>
      
      <Section title="Ignore Patterns">
        <p className="text-xs text-muted-foreground mb-4">Files and folders matching these patterns will not be synchronized.</p>
        <div className="space-y-3">
          <div className="flex gap-2">
            <input 
              type="text"
              value={newPattern}
              onChange={(e) => onNewPatternChange(e.target.value)}
              placeholder="*.log, node_modules/, .git/"
              className="settings-input flex-1 font-mono"
              onKeyDown={(e) => e.key === 'Enter' && onAddPattern()}
            />
            <button 
              onClick={onAddPattern}
              disabled={!newPattern.trim()}
              className="settings-btn settings-btn-primary"
            >
              <Plus className="w-4 h-4" /> Add
            </button>
          </div>
          {ignorePatterns.length > 0 ? (
            <div className="space-y-2">
              {ignorePatterns.map((pattern, i) => (
                <div key={i} className="settings-pattern-item">
                  <div className="flex items-center gap-2">
                    <FileX className="w-4 h-4 text-muted-foreground" />
                    <span className="font-mono text-sm">{pattern}</span>
                  </div>
                  <button 
                    onClick={() => onRemovePattern(pattern)}
                    className="settings-pattern-remove-btn"
                  >
                    <X className="w-4 h-4" />
                  </button>
                </div>
              ))}
            </div>
          ) : (
            <p className="settings-empty-state">No ignore patterns defined</p>
          )}
        </div>
      </Section>
      
      <Section title="Export / Import">
        <div className="grid grid-cols-2 gap-4">
          <button onClick={onExportConfig} className="settings-btn settings-btn-secondary py-3.5">
            <Download className="w-4 h-4" /> Export Config
          </button>
          <button onClick={onImportConfig} className="settings-btn settings-btn-secondary py-3.5">
            <Upload className="w-4 h-4" /> Import Config
          </button>
        </div>
        <p className="text-xs text-muted-foreground mt-3">Export settings to share with other devices or backup your configuration.</p>
      </Section>
      
      <Section title="Support Bundle">
        <p className="text-xs text-muted-foreground mb-4">
          Generate a support bundle containing config, logs, and system info for troubleshooting.
        </p>
        <button onClick={onExportSupportBundle} className="w-full settings-btn settings-btn-outline py-3.5">
          <Download className="w-4 h-4" /> Export Support Bundle
        </button>
      </Section>
      
      <Section title="ML Anomaly Detection">
        <div className="p-4 rounded-lg bg-amber-500/10 border border-amber-500/30">
          <div className="flex items-center gap-2 text-amber-400 mb-2">
            <AlertTriangle className="w-4 h-4" />
            <span className="text-sm font-semibold">Experimental Feature</span>
          </div>
          <p className="text-xs text-muted-foreground">ML-based anomaly detection uses IsolationForest via ONNX Runtime to identify suspicious peer behavior. This feature is in beta.</p>
        </div>
      </Section>
      
      <Section title="Maintenance">
        <div className="grid grid-cols-2 gap-3">
          <button onClick={onRefreshStatus} className="settings-btn settings-btn-secondary py-3">
            <RefreshCw className="w-4 h-4" />
            Refresh Status
          </button>
          <button onClick={onResetDefaults} className="settings-btn settings-btn-destructive py-3">
            <RotateCcw className="w-4 h-4" />
            Reset Defaults
          </button>
        </div>
        <p className="text-xs text-muted-foreground mt-3">
          Reset will restore all settings to their default values. This action cannot be undone.
        </p>
      </Section>
    </div>
  )
}
