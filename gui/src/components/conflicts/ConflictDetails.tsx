import { 
  FileText, Check, AlertTriangle, Monitor, Globe, 
  RefreshCw, History, Layers, Eye, RotateCcw 
} from 'lucide-react'
import { ConflictDetail, FileVersion, formatSize, formatTime, getFileName, getChangeTypeColor } from './types'

interface ConflictDetailsProps {
  conflict: ConflictDetail | null
  onResolve: (conflictId: number, resolution: 'local' | 'remote' | 'both') => void
  onRestoreVersion?: (conflictId: number, versionId: number) => void
  onPreviewVersion?: (filePath: string, versionId: number) => void
}

export function ConflictDetails({ conflict, onResolve, onRestoreVersion, onPreviewVersion }: ConflictDetailsProps) {
  if (!conflict) {
    return (
      <div className="conflict-details-empty">
        <AlertTriangle className="w-12 h-12 mb-3 opacity-30" />
        <p>Select a conflict to view details</p>
      </div>
    )
  }

  return (
    <div className="space-y-5">
      {/* File Info Header */}
      <div className="flex items-start gap-3">
        <div className="conflict-details-file-icon">
          <FileText className="w-6 h-6 text-primary" />
        </div>
        <div className="flex-1">
          <h3 className="font-semibold text-lg">{getFileName(conflict.path)}</h3>
          <p className="text-xs text-muted-foreground break-all">{conflict.path}</p>
        </div>
      </div>

      {/* Version Comparison */}
      <div className="grid grid-cols-2 gap-4">
        {/* Local Version */}
        <div className="conflict-version-card conflict-version-local">
          <div className="conflict-version-header">
            <Monitor className="w-5 h-5 text-blue-400" />
            <span className="font-semibold text-blue-400">Local Version</span>
          </div>
          <div className="conflict-version-info">
            <div className="conflict-version-row conflict-version-row-local">
              <span className="text-muted-foreground">Size</span>
              <span className="font-mono font-medium">{formatSize(conflict.localSize)}</span>
            </div>
            <div className="conflict-version-row conflict-version-row-local">
              <span className="text-muted-foreground">Modified</span>
              <span className="text-xs">{formatTime(conflict.localTimestamp)}</span>
            </div>
            <div className="conflict-version-row-last">
              <span className="text-muted-foreground">Hash</span>
              <span className="font-mono text-xs">{conflict.localHash.substring(0, 12)}...</span>
            </div>
          </div>
          <button
            onClick={() => onResolve(conflict.id, 'local')}
            className="conflict-resolve-btn conflict-resolve-local"
          >
            <Check className="w-4 h-4" /> Keep Local
          </button>
        </div>

        {/* Remote Version */}
        <div className="conflict-version-card conflict-version-remote">
          <div className="conflict-version-header">
            <Globe className="w-5 h-5 text-green-400" />
            <span className="font-semibold text-green-400">Remote Version</span>
          </div>
          <div className="conflict-version-info">
            <div className="conflict-version-row conflict-version-row-remote">
              <span className="text-muted-foreground">Size</span>
              <span className="font-mono font-medium">{formatSize(conflict.remoteSize)}</span>
            </div>
            <div className="conflict-version-row conflict-version-row-remote">
              <span className="text-muted-foreground">Modified</span>
              <span className="text-xs">{formatTime(conflict.remoteTimestamp)}</span>
            </div>
            <div className="conflict-version-row conflict-version-row-remote">
              <span className="text-muted-foreground">From Peer</span>
              <span className="font-mono text-xs">
                {conflict.remotePeerName || conflict.remotePeerId.substring(0, 12)}
              </span>
            </div>
            <div className="conflict-version-row-last">
              <span className="text-muted-foreground">Hash</span>
              <span className="font-mono text-xs">{conflict.remoteHash.substring(0, 12)}...</span>
            </div>
          </div>
          <button
            onClick={() => onResolve(conflict.id, 'remote')}
            className="conflict-resolve-btn conflict-resolve-remote"
          >
            <Check className="w-4 h-4" /> Keep Remote
          </button>
        </div>
      </div>

      {/* Keep Both Option */}
      <button
        onClick={() => onResolve(conflict.id, 'both')}
        className="conflict-keep-both-btn"
      >
        <Layers className="w-4 h-4" /> Keep Both Versions
        <span className="text-xs text-muted-foreground ml-1">(creates .conflict copy)</span>
      </button>

      {/* Version History for this file */}
      {conflict.versions && conflict.versions.length > 0 && (
        <div className="mt-6">
          <h4 className="font-medium mb-3 flex items-center gap-2">
            <History className="w-4 h-4" />
            Version History
          </h4>
          <div className="space-y-2 max-h-40 overflow-y-auto">
            {conflict.versions.map((version) => (
              <div key={version.versionId} className="conflict-version-history-item">
                <div className="flex items-center gap-3">
                  <span className={`px-2 py-0.5 rounded-full text-xs ${getChangeTypeColor(version.changeType)}`}>
                    {version.changeType}
                  </span>
                  <span className="text-muted-foreground">{formatTime(version.timestamp)}</span>
                  <span className="text-xs">{formatSize(version.size)}</span>
                </div>
                <div className="flex items-center gap-1">
                  {onPreviewVersion && (
                    <button 
                      onClick={() => onPreviewVersion(conflict.path, version.versionId)}
                      className="conflict-version-action-btn"
                      title="Preview"
                    >
                      <Eye className="w-4 h-4" />
                    </button>
                  )}
                  {onRestoreVersion && (
                    <button 
                      onClick={() => onRestoreVersion(conflict.id, version.versionId)}
                      className="conflict-version-action-btn text-primary"
                      title="Restore"
                    >
                      <RotateCcw className="w-4 h-4" />
                    </button>
                  )}
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Strategy Info */}
      <div className="conflict-strategy-info">
        <RefreshCw className="w-4 h-4 mt-0.5" />
        <div>
          <strong className="text-foreground">Auto-Resolution Strategy:</strong>{' '}
          {conflict.strategy || 'Manual Resolution Required'}
          <p className="mt-1 opacity-70">
            You can choose a different resolution above or let the system handle it automatically.
          </p>
        </div>
      </div>
    </div>
  )
}
