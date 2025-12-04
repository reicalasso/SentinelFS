import { useState, useMemo } from 'react'
import { 
  X, FileText, Clock, HardDrive, Check, ArrowRight, 
  RefreshCw, History, AlertTriangle, Monitor, Globe,
  ChevronDown, ChevronRight, Filter, Search, Trash2,
  RotateCcw, Eye, Download, Layers
} from 'lucide-react'

// Types
export interface FileVersion {
  versionId: number
  filePath: string
  versionPath: string
  hash: string
  peerId: string
  timestamp: number
  size: number
  changeType: 'create' | 'modify' | 'conflict' | 'remote' | 'backup'
  comment?: string
}

export interface ConflictDetail {
  id: number
  path: string
  localHash: string
  remoteHash: string
  localSize: number
  remoteSize: number
  localTimestamp: number
  remoteTimestamp: number
  remotePeerId: string
  remotePeerName?: string
  strategy: string
  detectedAt: number
  resolved: boolean
  versions?: FileVersion[]
}

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

type ViewMode = 'conflicts' | 'versions'
type SortBy = 'time' | 'name' | 'size'

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
  const [selectedVersionFile, setSelectedVersionFile] = useState<string | null>(null)

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

  if (!isOpen) return null

  const formatSize = (bytes: number) => {
    if (bytes < 1024) return bytes + ' B'
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  }

  const formatTime = (timestamp: number) => {
    const date = new Date(timestamp)
    return date.toLocaleString()
  }

  const formatRelativeTime = (timestamp: number) => {
    const now = Date.now()
    const diff = now - timestamp
    const minutes = Math.floor(diff / 60000)
    const hours = Math.floor(minutes / 60)
    const days = Math.floor(hours / 24)
    
    if (minutes < 1) return 'Just now'
    if (minutes < 60) return `${minutes}m ago`
    if (hours < 24) return `${hours}h ago`
    if (days < 7) return `${days}d ago`
    return new Date(timestamp).toLocaleDateString()
  }

  const getFileName = (path: string) => path.split('/').pop() || path
  const getFileDir = (path: string) => {
    const parts = path.split('/')
    parts.pop()
    return parts.join('/') || '/'
  }

  const toggleFileExpand = (path: string) => {
    const newExpanded = new Set(expandedFiles)
    if (newExpanded.has(path)) {
      newExpanded.delete(path)
    } else {
      newExpanded.add(path)
    }
    setExpandedFiles(newExpanded)
  }

  const getChangeTypeColor = (type: string) => {
    switch (type) {
      case 'conflict': return 'text-red-400 bg-red-500/10'
      case 'remote': return 'text-green-400 bg-green-500/10'
      case 'modify': return 'text-blue-400 bg-blue-500/10'
      case 'backup': return 'text-yellow-400 bg-yellow-500/10'
      case 'create': return 'text-purple-400 bg-purple-500/10'
      default: return 'text-gray-400 bg-gray-500/10'
    }
  }

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm">
      <div className="bg-card border border-border rounded-2xl shadow-2xl w-full max-w-5xl max-h-[85vh] overflow-hidden animate-in zoom-in-95 duration-200">
        {/* Header */}
        <div className="flex items-center justify-between p-5 border-b border-border bg-gradient-to-r from-card to-secondary/30">
          <div className="flex items-center gap-4">
            <div className="p-2 bg-primary/10 rounded-xl">
              <AlertTriangle className="w-6 h-6 text-primary" />
            </div>
            <div>
              <h2 className="font-bold text-xl">Conflict Center</h2>
              <p className="text-xs text-muted-foreground mt-0.5">
                {conflicts.length} conflict{conflicts.length !== 1 ? 's' : ''} · {versionedFiles.size} files with versions
              </p>
            </div>
          </div>
          
          {/* View Mode Toggle */}
          <div className="flex items-center gap-2">
            <div className="flex bg-secondary/50 rounded-lg p-1">
              <button
                onClick={() => setViewMode('conflicts')}
                className={`px-4 py-1.5 rounded-md text-sm font-medium transition-all ${
                  viewMode === 'conflicts' 
                    ? 'bg-primary text-primary-foreground shadow-sm' 
                    : 'text-muted-foreground hover:text-foreground'
                }`}
              >
                <AlertTriangle className="w-4 h-4 inline mr-1.5" />
                Conflicts
              </button>
              <button
                onClick={() => setViewMode('versions')}
                className={`px-4 py-1.5 rounded-md text-sm font-medium transition-all ${
                  viewMode === 'versions' 
                    ? 'bg-primary text-primary-foreground shadow-sm' 
                    : 'text-muted-foreground hover:text-foreground'
                }`}
              >
                <History className="w-4 h-4 inline mr-1.5" />
                Versions
              </button>
            </div>
            <button onClick={onClose} className="p-2 hover:bg-secondary rounded-lg ml-2">
              <X className="w-5 h-5" />
            </button>
          </div>
        </div>

        {/* Search & Filter Bar */}
        <div className="flex items-center gap-3 px-5 py-3 border-b border-border bg-secondary/20">
          <div className="flex-1 relative">
            <Search className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-muted-foreground" />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search files..."
              className="w-full pl-10 pr-4 py-2 bg-secondary/50 border border-border rounded-lg text-sm focus:outline-none focus:ring-2 focus:ring-primary/50"
            />
          </div>
          <div className="flex items-center gap-2">
            <Filter className="w-4 h-4 text-muted-foreground" />
            <select
              value={sortBy}
              onChange={(e) => setSortBy(e.target.value as SortBy)}
              className="px-3 py-2 bg-secondary/50 border border-border rounded-lg text-sm focus:outline-none"
            >
              <option value="time">Sort by Time</option>
              <option value="name">Sort by Name</option>
              <option value="size">Sort by Size</option>
            </select>
          </div>
        </div>

        {viewMode === 'conflicts' ? (
          /* Conflicts View */
          <div className="flex h-[550px]">
            {/* Conflict List */}
            <div className="w-2/5 border-r border-border overflow-y-auto">
              {filteredConflicts.length === 0 ? (
                <div className="h-full flex flex-col items-center justify-center text-muted-foreground p-6">
                  <Check className="w-12 h-12 mb-3 text-green-500" />
                  <p className="font-medium">No conflicts!</p>
                  <p className="text-xs text-center mt-1">All files are in sync</p>
                </div>
              ) : (
                filteredConflicts.map(conflict => (
                  <div
                    key={conflict.id}
                    onClick={() => setSelectedConflict(conflict)}
                    className={`p-4 cursor-pointer border-b border-border/50 hover:bg-secondary/50 transition-colors ${
                      selectedConflict?.id === conflict.id ? 'bg-secondary border-l-2 border-l-primary' : ''
                    }`}
                  >
                    <div className="flex items-start gap-3">
                      <div className="p-2 bg-yellow-500/10 rounded-lg mt-0.5">
                        <AlertTriangle className="w-4 h-4 text-yellow-500" />
                      </div>
                      <div className="flex-1 min-w-0">
                        <div className="flex items-center gap-2">
                          <span className="text-sm font-medium truncate">{getFileName(conflict.path)}</span>
                        </div>
                        <p className="text-xs text-muted-foreground truncate mt-0.5">{getFileDir(conflict.path)}</p>
                        <div className="flex items-center gap-3 mt-2 text-xs">
                          <span className="flex items-center gap-1 text-blue-400">
                            <Monitor className="w-3 h-3" />
                            {formatSize(conflict.localSize)}
                          </span>
                          <span className="text-muted-foreground">vs</span>
                          <span className="flex items-center gap-1 text-green-400">
                            <Globe className="w-3 h-3" />
                            {formatSize(conflict.remoteSize)}
                          </span>
                        </div>
                        <div className="flex items-center gap-2 mt-2">
                          <span className="text-[10px] text-muted-foreground">
                            {formatRelativeTime(conflict.detectedAt)}
                          </span>
                          <span className="text-[10px] px-1.5 py-0.5 bg-secondary rounded-full">
                            from {conflict.remotePeerName || conflict.remotePeerId.substring(0, 8)}
                          </span>
                        </div>
                      </div>
                    </div>
                  </div>
                ))
              )}
            </div>

            {/* Conflict Details */}
            <div className="flex-1 p-5 overflow-y-auto bg-gradient-to-br from-transparent to-secondary/10">
              {selectedConflict ? (
                <div className="space-y-5">
                  {/* File Info Header */}
                  <div className="flex items-start gap-3">
                    <div className="p-3 bg-primary/10 rounded-xl">
                      <FileText className="w-6 h-6 text-primary" />
                    </div>
                    <div className="flex-1">
                      <h3 className="font-semibold text-lg">{getFileName(selectedConflict.path)}</h3>
                      <p className="text-xs text-muted-foreground break-all">{selectedConflict.path}</p>
                    </div>
                  </div>

                  {/* Version Comparison */}
                  <div className="grid grid-cols-2 gap-4">
                    {/* Local Version */}
                    <div className="p-4 rounded-xl border-2 border-blue-500/30 bg-gradient-to-br from-blue-500/5 to-blue-500/10">
                      <div className="flex items-center gap-2 mb-4">
                        <Monitor className="w-5 h-5 text-blue-400" />
                        <span className="font-semibold text-blue-400">Local Version</span>
                      </div>
                      <div className="space-y-3 text-sm">
                        <div className="flex justify-between items-center py-2 border-b border-blue-500/20">
                          <span className="text-muted-foreground">Size</span>
                          <span className="font-mono font-medium">{formatSize(selectedConflict.localSize)}</span>
                        </div>
                        <div className="flex justify-between items-center py-2 border-b border-blue-500/20">
                          <span className="text-muted-foreground">Modified</span>
                          <span className="text-xs">{formatTime(selectedConflict.localTimestamp)}</span>
                        </div>
                        <div className="flex justify-between items-center py-2">
                          <span className="text-muted-foreground">Hash</span>
                          <span className="font-mono text-xs">{selectedConflict.localHash.substring(0, 12)}...</span>
                        </div>
                      </div>
                      <button
                        onClick={() => onResolve(selectedConflict.id, 'local')}
                        className="w-full mt-4 py-2.5 bg-blue-600 hover:bg-blue-700 text-white rounded-lg font-medium transition-colors flex items-center justify-center gap-2 shadow-lg shadow-blue-500/20"
                      >
                        <Check className="w-4 h-4" /> Keep Local
                      </button>
                    </div>

                    {/* Remote Version */}
                    <div className="p-4 rounded-xl border-2 border-green-500/30 bg-gradient-to-br from-green-500/5 to-green-500/10">
                      <div className="flex items-center gap-2 mb-4">
                        <Globe className="w-5 h-5 text-green-400" />
                        <span className="font-semibold text-green-400">Remote Version</span>
                      </div>
                      <div className="space-y-3 text-sm">
                        <div className="flex justify-between items-center py-2 border-b border-green-500/20">
                          <span className="text-muted-foreground">Size</span>
                          <span className="font-mono font-medium">{formatSize(selectedConflict.remoteSize)}</span>
                        </div>
                        <div className="flex justify-between items-center py-2 border-b border-green-500/20">
                          <span className="text-muted-foreground">Modified</span>
                          <span className="text-xs">{formatTime(selectedConflict.remoteTimestamp)}</span>
                        </div>
                        <div className="flex justify-between items-center py-2 border-b border-green-500/20">
                          <span className="text-muted-foreground">From Peer</span>
                          <span className="font-mono text-xs">
                            {selectedConflict.remotePeerName || selectedConflict.remotePeerId.substring(0, 12)}
                          </span>
                        </div>
                        <div className="flex justify-between items-center py-2">
                          <span className="text-muted-foreground">Hash</span>
                          <span className="font-mono text-xs">{selectedConflict.remoteHash.substring(0, 12)}...</span>
                        </div>
                      </div>
                      <button
                        onClick={() => onResolve(selectedConflict.id, 'remote')}
                        className="w-full mt-4 py-2.5 bg-green-600 hover:bg-green-700 text-white rounded-lg font-medium transition-colors flex items-center justify-center gap-2 shadow-lg shadow-green-500/20"
                      >
                        <Check className="w-4 h-4" /> Keep Remote
                      </button>
                    </div>
                  </div>

                  {/* Keep Both Option */}
                  <button
                    onClick={() => onResolve(selectedConflict.id, 'both')}
                    className="w-full py-3.5 bg-gradient-to-r from-secondary to-secondary/60 hover:from-secondary/80 hover:to-secondary/40 text-foreground rounded-xl font-medium transition-all flex items-center justify-center gap-2 border border-border"
                  >
                    <Layers className="w-4 h-4" /> Keep Both Versions
                    <span className="text-xs text-muted-foreground ml-1">(creates .conflict copy)</span>
                  </button>

                  {/* Version History for this file */}
                  {selectedConflict.versions && selectedConflict.versions.length > 0 && (
                    <div className="mt-6">
                      <h4 className="font-medium mb-3 flex items-center gap-2">
                        <History className="w-4 h-4" />
                        Version History
                      </h4>
                      <div className="space-y-2 max-h-40 overflow-y-auto">
                        {selectedConflict.versions.map((version) => (
                          <div 
                            key={version.versionId}
                            className="flex items-center justify-between p-3 bg-secondary/30 rounded-lg text-sm"
                          >
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
                                  onClick={() => onPreviewVersion(selectedConflict.path, version.versionId)}
                                  className="p-1.5 hover:bg-secondary rounded-md"
                                  title="Preview"
                                >
                                  <Eye className="w-4 h-4" />
                                </button>
                              )}
                              {onRestoreVersion && (
                                <button 
                                  onClick={() => onRestoreVersion(selectedConflict.id, version.versionId)}
                                  className="p-1.5 hover:bg-secondary rounded-md text-primary"
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
                  <div className="text-xs text-muted-foreground p-4 bg-secondary/30 rounded-xl flex items-start gap-2">
                    <RefreshCw className="w-4 h-4 mt-0.5" />
                    <div>
                      <strong className="text-foreground">Auto-Resolution Strategy:</strong>{' '}
                      {selectedConflict.strategy || 'Manual Resolution Required'}
                      <p className="mt-1 opacity-70">
                        You can choose a different resolution above or let the system handle it automatically.
                      </p>
                    </div>
                  </div>
                </div>
              ) : (
                <div className="h-full flex flex-col items-center justify-center text-muted-foreground">
                  <AlertTriangle className="w-12 h-12 mb-3 opacity-30" />
                  <p>Select a conflict to view details</p>
                </div>
              )}
            </div>
          </div>
        ) : (
          /* Versions View */
          <div className="h-[550px] overflow-y-auto p-5">
            {versionedFilesList.length === 0 ? (
              <div className="h-full flex flex-col items-center justify-center text-muted-foreground">
                <History className="w-12 h-12 mb-3 opacity-30" />
                <p className="font-medium">No version history</p>
                <p className="text-xs text-center mt-1">File versions will appear here when changes are made</p>
              </div>
            ) : (
              <div className="space-y-2">
                {versionedFilesList.map(([filePath, versions]) => (
                  <div key={filePath} className="border border-border rounded-xl overflow-hidden">
                    {/* File Header */}
                    <button
                      onClick={() => toggleFileExpand(filePath)}
                      className="w-full flex items-center gap-3 p-4 hover:bg-secondary/50 transition-colors"
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
                      <span className="px-2 py-1 bg-secondary rounded-full text-xs">
                        {versions.length} version{versions.length !== 1 ? 's' : ''}
                      </span>
                    </button>

                    {/* Expanded Versions */}
                    {expandedFiles.has(filePath) && (
                      <div className="border-t border-border bg-secondary/10 p-4 space-y-2">
                        {versions.sort((a, b) => b.timestamp - a.timestamp).map((version) => (
                          <div 
                            key={version.versionId}
                            className="flex items-center justify-between p-3 bg-card rounded-lg border border-border hover:border-primary/30 transition-colors"
                          >
                            <div className="flex items-center gap-4">
                              <span className={`px-2.5 py-1 rounded-full text-xs font-medium ${getChangeTypeColor(version.changeType)}`}>
                                {version.changeType}
                              </span>
                              <div>
                                <p className="text-sm font-medium">{formatTime(version.timestamp)}</p>
                                <div className="flex items-center gap-3 text-xs text-muted-foreground mt-0.5">
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
                                  className="p-2 hover:bg-secondary rounded-lg transition-colors"
                                  title="Preview version"
                                >
                                  <Eye className="w-4 h-4" />
                                </button>
                              )}
                              <button 
                                onClick={() => {
                                  // Download version
                                  const link = document.createElement('a')
                                  link.href = version.versionPath
                                  link.download = `${getFileName(filePath)}.v${version.versionId}`
                                  link.click()
                                }}
                                className="p-2 hover:bg-secondary rounded-lg transition-colors"
                                title="Download version"
                              >
                                <Download className="w-4 h-4" />
                              </button>
                              {onRestoreVersion && (
                                <button 
                                  onClick={() => onRestoreVersion(0, version.versionId)}
                                  className="p-2 hover:bg-primary/10 text-primary rounded-lg transition-colors"
                                  title="Restore this version"
                                >
                                  <RotateCcw className="w-4 h-4" />
                                </button>
                              )}
                              {onDeleteVersion && (
                                <button 
                                  onClick={() => onDeleteVersion(filePath, version.versionId)}
                                  className="p-2 hover:bg-red-500/10 text-red-400 rounded-lg transition-colors"
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
            )}
          </div>
        )}

        {/* Footer */}
        <div className="flex items-center justify-between px-5 py-3 border-t border-border bg-secondary/20">
          <div className="text-xs text-muted-foreground">
            {viewMode === 'conflicts' 
              ? `${filteredConflicts.length} of ${conflicts.length} conflicts shown`
              : `${versionedFilesList.length} files with version history`
            }
          </div>
          <div className="flex items-center gap-2">
            {viewMode === 'conflicts' && filteredConflicts.length > 0 && (
              <button
                onClick={() => {
                  // Resolve all with newest wins
                  filteredConflicts.forEach(c => {
                    const resolution = c.remoteTimestamp > c.localTimestamp ? 'remote' : 'local'
                    onResolve(c.id, resolution)
                  })
                }}
                className="px-4 py-2 bg-secondary hover:bg-secondary/80 rounded-lg text-sm font-medium transition-colors"
              >
                Resolve All (Newest Wins)
              </button>
            )}
            <button
              onClick={onClose}
              className="px-4 py-2 bg-primary text-primary-foreground rounded-lg text-sm font-medium hover:bg-primary/90 transition-colors"
            >
              Done
            </button>
          </div>
        </div>
      </div>
    </div>
  )
}
