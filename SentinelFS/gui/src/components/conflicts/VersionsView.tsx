import { 
  FileText, History, ChevronDown, ChevronRight, 
  Globe, Eye, Download, RotateCcw, Trash2 
} from 'lucide-react'
import { FileVersion, formatSize, formatTime, getFileName, getChangeTypeColor } from './types'

interface VersionsViewProps {
  versionedFilesList: [string, FileVersion[]][]
  expandedFiles: Set<string>
  onToggleExpand: (path: string) => void
  onPreviewVersion?: (filePath: string, versionId: number) => void
  onRestoreVersion?: (conflictId: number, versionId: number) => void
  onDeleteVersion?: (filePath: string, versionId: number) => void
}

export function VersionsView({ 
  versionedFilesList, 
  expandedFiles, 
  onToggleExpand,
  onPreviewVersion,
  onRestoreVersion,
  onDeleteVersion
}: VersionsViewProps) {
  if (versionedFilesList.length === 0) {
    return (
      <div className="versions-view-empty">
        <History className="w-12 h-12 mb-3 opacity-30" />
        <p className="font-medium">No version history</p>
        <p className="text-xs text-center mt-1">File versions will appear here when changes are made</p>
      </div>
    )
  }

  return (
    <div className="space-y-2">
      {versionedFilesList.map(([filePath, versions]) => (
        <div key={filePath} className="versions-file-card">
          {/* File Header */}
          <button
            onClick={() => onToggleExpand(filePath)}
            className="versions-file-header"
          >
            {expandedFiles.has(filePath) ? (
              <ChevronDown className="w-4 h-4" />
            ) : (
              <ChevronRight className="w-4 h-4" />
            )}
            <FileText className="w-5 h-5 text-primary" />
            <div className="flex-1 text-left">
              <p className="font-medium">{getFileName(filePath)}</p>
              <p className="text-xs text-muted-foreground truncate">{filePath}</p>
            </div>
            <span className="versions-count-badge">
              {versions.length} version{versions.length !== 1 ? 's' : ''}
            </span>
          </button>

          {/* Expanded Versions */}
          {expandedFiles.has(filePath) && (
            <div className="versions-expanded">
              {versions.sort((a, b) => b.timestamp - a.timestamp).map((version) => (
                <div key={version.versionId} className="version-item">
                  <div className="flex items-center gap-4">
                    <span className={`px-2.5 py-1 rounded-full text-xs font-medium ${getChangeTypeColor(version.changeType)}`}>
                      {version.changeType}
                    </span>
                    <div>
                      <p className="text-sm font-medium">{formatTime(version.timestamp)}</p>
                      <div className="version-item-meta">
                        <span>{formatSize(version.size)}</span>
                        <span className="font-mono">{version.hash.substring(0, 12)}</span>
                        {version.peerId && (
                          <span className="flex items-center gap-1">
                            <Globe className="w-3 h-3" />
                            {version.peerId.substring(0, 8)}
                          </span>
                        )}
                      </div>
                    </div>
                  </div>
                  <div className="flex items-center gap-1">
                    {onPreviewVersion && (
                      <button 
                        onClick={() => onPreviewVersion(filePath, version.versionId)}
                        className="version-action-btn"
                        title="Preview version"
                      >
                        <Eye className="w-4 h-4" />
                      </button>
                    )}
                    <button 
                      onClick={() => {
                        const link = document.createElement('a')
                        link.href = version.versionPath
                        link.download = `${getFileName(filePath)}.v${version.versionId}`
                        link.click()
                      }}
                      className="version-action-btn"
                      title="Download version"
                    >
                      <Download className="w-4 h-4" />
                    </button>
                    {onRestoreVersion && (
                      <button 
                        onClick={() => onRestoreVersion(0, version.versionId)}
                        className="version-action-btn version-action-restore"
                        title="Restore this version"
                      >
                        <RotateCcw className="w-4 h-4" />
                      </button>
                    )}
                    {onDeleteVersion && (
                      <button 
                        onClick={() => onDeleteVersion(filePath, version.versionId)}
                        className="version-action-btn version-action-delete"
                        title="Delete this version"
                      >
                        <Trash2 className="w-4 h-4" />
                      </button>
                    )}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      ))}
    </div>
  )
}
