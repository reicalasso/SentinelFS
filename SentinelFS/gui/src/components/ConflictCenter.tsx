import { useState, useMemo } from 'react'
import { 
  ConflictHeader, 
  ConflictSearchBar, 
  ConflictList, 
  ConflictDetails, 
  VersionsView,
  ConflictFooter,
  FileVersion,
  ConflictDetail,
  ViewMode,
  SortBy
} from './conflicts'

// Re-export types for external use
export type { FileVersion, ConflictDetail }

interface ConflictCenterProps {
  conflicts: ConflictDetail[]
  isOpen: boolean
  onClose: () => void
  onResolve: (conflictId: number, resolution: 'local' | 'remote' | 'both') => void
  onRestoreVersion?: (conflictId: number, versionId: number) => void
  onPreviewVersion?: (filePath: string, versionId: number) => void
  onDeleteVersion?: (filePath: string, versionId: number) => void
  versionedFiles?: Map<string, FileVersion[]>
}

export function ConflictCenter({ 
  conflicts, 
  isOpen, 
  onClose, 
  onResolve,
  onRestoreVersion,
  onPreviewVersion,
  onDeleteVersion,
  versionedFiles = new Map()
}: ConflictCenterProps) {
  const [selectedConflict, setSelectedConflict] = useState<ConflictDetail | null>(
    conflicts.length > 0 ? conflicts[0] : null
  )
  const [viewMode, setViewMode] = useState<ViewMode>('conflicts')
  const [searchQuery, setSearchQuery] = useState('')
  const [sortBy, setSortBy] = useState<SortBy>('time')
  const [expandedFiles, setExpandedFiles] = useState<Set<string>>(new Set())

  // Filter and sort conflicts
  const filteredConflicts = useMemo(() => {
    let filtered = conflicts.filter(c => 
      c.path.toLowerCase().includes(searchQuery.toLowerCase())
    )
    
    switch (sortBy) {
      case 'time':
        return filtered.sort((a, b) => b.detectedAt - a.detectedAt)
      case 'name':
        return filtered.sort((a, b) => a.path.localeCompare(b.path))
      case 'size':
        return filtered.sort((a, b) => Math.max(b.localSize, b.remoteSize) - Math.max(a.localSize, a.remoteSize))
      default:
        return filtered
    }
  }, [conflicts, searchQuery, sortBy])

  // Get versioned files list
  const versionedFilesList = useMemo(() => {
    const files = Array.from(versionedFiles.entries())
    return files.filter(([path]) => 
      path.toLowerCase().includes(searchQuery.toLowerCase())
    )
  }, [versionedFiles, searchQuery])

  const toggleFileExpand = (path: string) => {
    const newExpanded = new Set(expandedFiles)
    if (newExpanded.has(path)) {
      newExpanded.delete(path)
    } else {
      newExpanded.add(path)
    }
    setExpandedFiles(newExpanded)
  }

  if (!isOpen) return null

  return (
    <div className="conflict-modal-overlay">
      <div className="conflict-modal">
        <ConflictHeader 
          conflictCount={conflicts.length}
          versionedFilesCount={versionedFiles.size}
          viewMode={viewMode}
          onViewModeChange={setViewMode}
          onClose={onClose}
        />

        <ConflictSearchBar 
          searchQuery={searchQuery}
          onSearchChange={setSearchQuery}
          sortBy={sortBy}
          onSortChange={setSortBy}
        />

        {viewMode === 'conflicts' ? (
          <div className="conflict-main-content">
            <div className="conflict-list-panel">
              <ConflictList 
                conflicts={filteredConflicts}
                selectedConflict={selectedConflict}
                onSelect={setSelectedConflict}
              />
            </div>

            <div className="conflict-details-panel">
              <ConflictDetails 
                conflict={selectedConflict}
                onResolve={onResolve}
                onRestoreVersion={onRestoreVersion}
                onPreviewVersion={onPreviewVersion}
              />
            </div>
          </div>
        ) : (
          <div className="versions-main-content">
            <VersionsView 
              versionedFilesList={versionedFilesList}
              expandedFiles={expandedFiles}
              onToggleExpand={toggleFileExpand}
              onPreviewVersion={onPreviewVersion}
              onRestoreVersion={onRestoreVersion}
              onDeleteVersion={onDeleteVersion}
            />
          </div>
        )}

        <ConflictFooter 
          viewMode={viewMode}
          filteredCount={filteredConflicts.length}
          totalCount={conflicts.length}
          versionedFilesCount={versionedFilesList.length}
          conflicts={filteredConflicts}
          onResolve={onResolve}
          onClose={onClose}
        />
      </div>
    </div>
  )
}
