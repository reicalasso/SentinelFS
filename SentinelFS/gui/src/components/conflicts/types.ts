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

export type ViewMode = 'conflicts' | 'versions'
export type SortBy = 'time' | 'name' | 'size'

// Utility functions
export const formatSize = (bytes: number) => {
  if (bytes < 1024) return bytes + ' B'
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
  if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
  return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
}

export const formatTime = (timestamp: number) => {
  const date = new Date(timestamp)
  return date.toLocaleString()
}

export const formatRelativeTime = (timestamp: number) => {
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

export const getFileName = (path: string) => path.split('/').pop() || path

export const getFileDir = (path: string) => {
  const parts = path.split('/')
  parts.pop()
  return parts.join('/') || '/'
}

export const getChangeTypeColor = (type: string) => {
  switch (type) {
    case 'conflict': return 'change-delete'
    case 'remote': return 'change-add'
    case 'modify': return 'change-rename'
    case 'backup': return 'change-conflict'
    case 'create': return 'change-modify'
    default: return 'text-muted-foreground bg-muted/10'
  }
}
