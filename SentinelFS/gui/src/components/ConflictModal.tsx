import { useState } from 'react'
import { X, FileText, Clock, HardDrive, Check, ArrowRight } from 'lucide-react'

interface Conflict {
  id: number
  path: string
  localSize: number
  remoteSize: number
  localTimestamp: string
  remoteTimestamp: string
  remotePeerId: string
  strategy: string
}

interface ConflictModalProps {
  conflicts: Conflict[]
  isOpen: boolean
  onClose: () => void
  onResolve: (conflictId: number, resolution: 'local' | 'remote' | 'both') => void
}

export function ConflictModal({ conflicts, isOpen, onClose, onResolve }: ConflictModalProps) {
  const [selectedConflict, setSelectedConflict] = useState<Conflict | null>(
    conflicts.length > 0 ? conflicts[0] : null
  )

  if (!isOpen) return null

  const formatSize = (bytes: number) => {
    if (bytes < 1024) return bytes + ' B'
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  }

  const formatTime = (timestamp: string) => {
    const date = new Date(timestamp)
    return date.toLocaleString()
  }

  const getFileName = (path: string) => path.split('/').pop() || path

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 backdrop-blur-sm">
      <div className="bg-card border border-border rounded-xl shadow-2xl w-full max-w-2xl max-h-[80vh] overflow-hidden animate-in zoom-in-95 duration-200">
        {/* Header */}
        <div className="flex items-center justify-between p-4 border-b border-border">
          <div>
            <h2 className="font-semibold text-lg">Resolve Conflicts</h2>
            <p className="text-xs text-muted-foreground">{conflicts.length} conflict(s) need resolution</p>
          </div>
          <button onClick={onClose} className="p-2 hover:bg-secondary rounded-lg">
            <X className="w-5 h-5" />
          </button>
        </div>

        <div className="flex h-[500px]">
          {/* Conflict List */}
          <div className="w-1/3 border-r border-border overflow-y-auto">
            {conflicts.map(conflict => (
              <div
                key={conflict.id}
                onClick={() => setSelectedConflict(conflict)}
                className={`p-3 cursor-pointer border-b border-border hover:bg-secondary/50 transition-colors ${
                  selectedConflict?.id === conflict.id ? 'bg-secondary' : ''
                }`}
              >
                <div className="flex items-center gap-2">
                  <FileText className="w-4 h-4 text-yellow-500" />
                  <span className="text-sm font-medium truncate">{getFileName(conflict.path)}</span>
                </div>
                <p className="text-xs text-muted-foreground mt-1 truncate">{conflict.path}</p>
              </div>
            ))}
          </div>

          {/* Conflict Details */}
          <div className="flex-1 p-4 overflow-y-auto">
            {selectedConflict ? (
              <div className="space-y-6">
                <div>
                  <h3 className="font-medium mb-2">{getFileName(selectedConflict.path)}</h3>
                  <p className="text-xs text-muted-foreground">{selectedConflict.path}</p>
                </div>

                {/* Comparison */}
                <div className="grid grid-cols-2 gap-4">
                  {/* Local Version */}
                  <div className="p-4 rounded-lg border border-blue-500/20 bg-blue-500/5">
                    <div className="flex items-center gap-2 mb-3">
                      <HardDrive className="w-4 h-4 text-blue-500" />
                      <span className="font-medium text-sm text-blue-500">Local Version</span>
                    </div>
                    <div className="space-y-2 text-xs">
                      <div className="flex justify-between">
                        <span className="text-muted-foreground">Size:</span>
                        <span className="font-mono">{formatSize(selectedConflict.localSize)}</span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-muted-foreground">Modified:</span>
                        <span>{formatTime(selectedConflict.localTimestamp)}</span>
                      </div>
                    </div>
                    <button
                      onClick={() => onResolve(selectedConflict.id, 'local')}
                      className="w-full mt-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg text-sm font-medium transition-colors flex items-center justify-center gap-2"
                    >
                      <Check className="w-4 h-4" /> Keep Local
                    </button>
                  </div>

                  {/* Remote Version */}
                  <div className="p-4 rounded-lg border border-green-500/20 bg-green-500/5">
                    <div className="flex items-center gap-2 mb-3">
                      <HardDrive className="w-4 h-4 text-green-500" />
                      <span className="font-medium text-sm text-green-500">Remote Version</span>
                    </div>
                    <div className="space-y-2 text-xs">
                      <div className="flex justify-between">
                        <span className="text-muted-foreground">Size:</span>
                        <span className="font-mono">{formatSize(selectedConflict.remoteSize)}</span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-muted-foreground">Modified:</span>
                        <span>{formatTime(selectedConflict.remoteTimestamp)}</span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-muted-foreground">From:</span>
                        <span className="font-mono">{selectedConflict.remotePeerId.substring(0, 12)}</span>
                      </div>
                    </div>
                    <button
                      onClick={() => onResolve(selectedConflict.id, 'remote')}
                      className="w-full mt-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg text-sm font-medium transition-colors flex items-center justify-center gap-2"
                    >
                      <Check className="w-4 h-4" /> Keep Remote
                    </button>
                  </div>
                </div>

                {/* Keep Both Option */}
                <button
                  onClick={() => onResolve(selectedConflict.id, 'both')}
                  className="w-full py-3 bg-secondary hover:bg-secondary/80 text-foreground rounded-lg text-sm font-medium transition-colors flex items-center justify-center gap-2"
                >
                  <ArrowRight className="w-4 h-4" /> Keep Both (rename remote)
                </button>

                {/* Strategy Info */}
                <div className="text-xs text-muted-foreground p-3 bg-secondary/30 rounded-lg">
                  <strong>Current Strategy:</strong> {selectedConflict.strategy || 'Manual Resolution'}
                </div>
              </div>
            ) : (
              <div className="h-full flex items-center justify-center text-muted-foreground">
                <p>Select a conflict to view details</p>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  )
}
