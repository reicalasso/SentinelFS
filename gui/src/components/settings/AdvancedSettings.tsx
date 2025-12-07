import { Database, Download, Upload, Plus, X, FileX } from 'lucide-react'
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
      <Section title="System Information">
        <div className="space-y-2 text-xs">
          <div className="flex justify-between py-1">
            <span className="text-muted-foreground">Metrics Port:</span>
            <span className="font-mono">{config?.metricsPort || 9100}</span>
          </div>
          <div className="flex justify-between py-1">
            <span className="text-muted-foreground">Database:</span>
            <span className="font-mono">sentinel.db (SQLite)</span>
          </div>
          <div className="flex justify-between py-1">
            <span className="text-muted-foreground">IPC Socket:</span>
            <span className="font-mono text-xs">/run/user/1000/sentinelfs/sentinel_daemon.sock</span>
          </div>
        </div>
      </Section>
      
      <Section title="Delta Sync Engine">
        <div className="space-y-2 text-xs">
          <div className="flex justify-between py-1">
            <span className="text-muted-foreground">Algorithm:</span>
            <span className="font-mono">Rolling Checksum (Adler32) + SHA-256</span>
          </div>
          <div className="flex justify-between py-1">
            <span className="text-muted-foreground">Block Size:</span>
            <span className="font-mono">4 KB</span>
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
        <div className="settings-warning-card">
          <div className="flex items-center gap-2 status-warning mb-2">
            <Database className="w-4 h-4" />
            <span className="text-sm font-semibold">Experimental Feature</span>
          </div>
          <p className="text-xs text-muted-foreground">ML-based anomaly detection uses IsolationForest via ONNX Runtime to identify suspicious peer behavior. This feature is in beta.</p>
        </div>
      </Section>
      
      <Section title="Danger Zone">
        <div className="space-y-3">
          <button onClick={onRefreshStatus} className="w-full settings-btn settings-btn-secondary py-3">
            Refresh Daemon Status
          </button>
          <button onClick={onResetDefaults} className="w-full settings-btn settings-btn-destructive py-3">
            Reset to Defaults
          </button>
        </div>
      </Section>
    </div>
  )
}
