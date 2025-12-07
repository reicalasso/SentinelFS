import { Check, AlertTriangle, Monitor, Globe } from 'lucide-react'
import { ConflictDetail, formatSize, formatRelativeTime, getFileName, getFileDir } from './types'

interface ConflictListProps {
  conflicts: ConflictDetail[]
  selectedConflict: ConflictDetail | null
  onSelect: (conflict: ConflictDetail) => void
}

export function ConflictList({ conflicts, selectedConflict, onSelect }: ConflictListProps) {
  if (conflicts.length === 0) {
    return (
      <div className="conflict-list-empty">
        <Check className="w-12 h-12 mb-3 text-green-500" />
        <p className="font-medium">No conflicts!</p>
        <p className="text-xs text-center mt-1">All files are in sync</p>
      </div>
    )
  }

  return (
    <>
      {conflicts.map(conflict => (
        <div
          key={conflict.id}
          onClick={() => onSelect(conflict)}
          className={`conflict-list-item ${selectedConflict?.id === conflict.id ? 'conflict-list-item-selected' : ''}`}
        >
          <div className="flex items-start gap-3">
            <div className="conflict-list-item-icon">
              <AlertTriangle className="w-4 h-4 text-yellow-500" />
            </div>
            <div className="flex-1 min-w-0">
              <div className="flex items-center gap-2">
                <span className="text-sm font-medium truncate">{getFileName(conflict.path)}</span>
              </div>
              <p className="text-xs text-muted-foreground truncate mt-0.5">{getFileDir(conflict.path)}</p>
              <div className="conflict-list-item-sizes">
                <span className="conflict-size-local">
                  <Monitor className="w-3 h-3" />
                  {formatSize(conflict.localSize)}
                </span>
                <span className="text-muted-foreground">vs</span>
                <span className="conflict-size-remote">
                  <Globe className="w-3 h-3" />
                  {formatSize(conflict.remoteSize)}
                </span>
              </div>
              <div className="conflict-list-item-meta">
                <span className="conflict-time">
                  {formatRelativeTime(conflict.detectedAt)}
                </span>
                <span className="conflict-peer-badge">
                  from {conflict.remotePeerName || conflict.remotePeerId.substring(0, 8)}
                </span>
              </div>
            </div>
          </div>
        </div>
      ))}
    </>
  )
}
