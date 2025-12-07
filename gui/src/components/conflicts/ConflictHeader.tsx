import { X, AlertTriangle, History } from 'lucide-react'
import { ViewMode } from './types'

interface ConflictHeaderProps {
  conflictCount: number
  versionedFilesCount: number
  viewMode: ViewMode
  onViewModeChange: (mode: ViewMode) => void
  onClose: () => void
}

export function ConflictHeader({ 
  conflictCount, 
  versionedFilesCount, 
  viewMode, 
  onViewModeChange, 
  onClose 
}: ConflictHeaderProps) {
  return (
    <div className="conflict-header">
      <div className="conflict-header-left">
        <div className="conflict-header-icon">
          <AlertTriangle className="w-5 h-5 sm:w-6 sm:h-6 text-primary" />
        </div>
        <div>
          <h2 className="conflict-header-title">Conflict Center</h2>
          <p className="conflict-header-subtitle">
            {conflictCount} conflict{conflictCount !== 1 ? 's' : ''} · {versionedFilesCount} files with versions
          </p>
        </div>
      </div>
      
      <div className="flex items-center gap-2">
        <div className="conflict-view-toggle">
          <button
            onClick={() => onViewModeChange('conflicts')}
            className={`conflict-view-btn ${viewMode === 'conflicts' ? 'conflict-view-btn-active' : ''}`}
          >
            <AlertTriangle className="w-3 h-3 sm:w-4 sm:h-4 inline mr-1 sm:mr-1.5" />
            <span className="hidden sm:inline">Conflicts</span>
          </button>
          <button
            onClick={() => onViewModeChange('versions')}
            className={`conflict-view-btn ${viewMode === 'versions' ? 'conflict-view-btn-active' : ''}`}
          >
            <History className="w-3 h-3 sm:w-4 sm:h-4 inline mr-1 sm:mr-1.5" />
            <span className="hidden sm:inline">Versions</span>
          </button>
        </div>
        <button onClick={onClose} className="conflict-close-btn">
          <X className="w-4 h-4 sm:w-5 sm:h-5" />
        </button>
      </div>
    </div>
  )
}
