import { ConflictDetail, ViewMode } from './types'

interface ConflictFooterProps {
  viewMode: ViewMode
  filteredCount: number
  totalCount: number
  versionedFilesCount: number
  conflicts: ConflictDetail[]
  onResolve: (conflictId: number, resolution: 'local' | 'remote' | 'both') => void
  onClose: () => void
}

export function ConflictFooter({ 
  viewMode, 
  filteredCount, 
  totalCount, 
  versionedFilesCount,
  conflicts,
  onResolve,
  onClose 
}: ConflictFooterProps) {
  const handleResolveAll = () => {
    conflicts.forEach(c => {
      const resolution = c.remoteTimestamp > c.localTimestamp ? 'remote' : 'local'
      onResolve(c.id, resolution)
    })
  }

  return (
    <div className="conflict-footer">
      <div className="conflict-footer-info">
        {viewMode === 'conflicts' 
          ? `${filteredCount} of ${totalCount} conflicts shown`
          : `${versionedFilesCount} files with version history`
        }
      </div>
      <div className="flex items-center gap-2">
        {viewMode === 'conflicts' && filteredCount > 0 && (
          <button onClick={handleResolveAll} className="conflict-resolve-all-btn">
            Resolve All (Newest Wins)
          </button>
        )}
        <button onClick={onClose} className="conflict-done-btn">
          Done
        </button>
      </div>
    </div>
  )
}
