export interface DetectedThreat {
  id: number
  filePath: string
  threatType: 'RANSOMWARE' | 'HIGH_ENTROPY' | 'MASS_OPERATION' | 'SUSPICIOUS_PATTERN' | 'UNKNOWN'
  threatLevel: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL'
  threatScore: number
  detectedAt: number
  entropy?: number
  fileSize: number
  hash?: string
  quarantinePath?: string
  mlModelUsed?: string
  additionalInfo?: string
  markedSafe?: boolean
}

export type ViewMode = 'all' | 'active' | 'safe'
export type SortBy = 'time' | 'severity' | 'name'

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
  if (minutes < 60) return `${minutes} min ago`
  if (hours < 24) return `${hours} hr ago`
  if (days < 7) return `${days} day${days > 1 ? 's' : ''} ago`
  return new Date(timestamp).toLocaleDateString('en-US')
}

export const getFileName = (path: string) => path.split('/').pop() || path

export const getThreatTypeLabel = (type: DetectedThreat['threatType']) => {
  switch (type) {
    case 'RANSOMWARE': return 'Ransomware'
    case 'HIGH_ENTROPY': return 'High Entropy'
    case 'MASS_OPERATION': return 'Mass Operation'
    case 'SUSPICIOUS_PATTERN': return 'Suspicious Pattern'
    case 'UNKNOWN': return 'Unknown'
  }
}

export const getThreatLevelColor = (level: DetectedThreat['threatLevel']) => {
  switch (level) {
    case 'CRITICAL': return 'text-red-600 dark:text-red-400'
    case 'HIGH': return 'text-orange-600 dark:text-orange-400'
    case 'MEDIUM': return 'text-yellow-600 dark:text-yellow-400'
    case 'LOW': return 'text-blue-600 dark:text-blue-400'
  }
}

export const getThreatLevelBgColor = (level: DetectedThreat['threatLevel']) => {
  switch (level) {
    case 'CRITICAL': return 'bg-red-500/10 border-red-500/20'
    case 'HIGH': return 'bg-orange-500/10 border-orange-500/20'
    case 'MEDIUM': return 'bg-yellow-500/10 border-yellow-500/20'
    case 'LOW': return 'bg-blue-500/10 border-blue-500/20'
  }
}
