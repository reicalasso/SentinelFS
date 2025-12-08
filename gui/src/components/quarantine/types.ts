// Zer0 Threat Types
export type Zer0ThreatType = 
  | 'NONE'
  | 'EXTENSION_MISMATCH'     // Extension doesn't match content
  | 'HIDDEN_EXECUTABLE'      // Executable disguised as other type
  | 'HIGH_ENTROPY_TEXT'      // Encrypted/obfuscated text file
  | 'RANSOMWARE_PATTERN'     // Ransomware behavior detected
  | 'MASS_MODIFICATION'      // Too many files changed quickly
  | 'SUSPICIOUS_RENAME'      // Suspicious rename pattern
  | 'KNOWN_MALWARE_HASH'     // Hash matches known malware
  | 'ANOMALOUS_BEHAVIOR'     // Unusual file access pattern
  | 'DOUBLE_EXTENSION'       // file.pdf.exe pattern
  | 'SCRIPT_IN_DATA'         // Script embedded in data file
  // Legacy types for backward compatibility
  | 'RANSOMWARE' 
  | 'HIGH_ENTROPY' 
  | 'MASS_OPERATION' 
  | 'SUSPICIOUS_PATTERN' 
  | 'UNKNOWN'

// Zer0 Threat Levels (extended)
export type Zer0ThreatLevel = 'NONE' | 'INFO' | 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL'

// Zer0 File Categories
export type FileCategory = 
  | 'UNKNOWN'
  | 'TEXT'
  | 'DOCUMENT'
  | 'IMAGE'
  | 'VIDEO'
  | 'AUDIO'
  | 'ARCHIVE'
  | 'EXECUTABLE'
  | 'DATABASE'
  | 'CONFIG'

export interface DetectedThreat {
  id: number
  filePath: string
  threatType: Zer0ThreatType
  threatLevel: Zer0ThreatLevel
  threatScore: number
  confidence?: number          // Zer0: 0.0 - 1.0
  detectedAt: number
  entropy?: number
  fileSize: number
  hash?: string
  quarantinePath?: string
  mlModelUsed?: string
  additionalInfo?: string
  markedSafe?: boolean
  // Zer0 specific fields
  fileCategory?: FileCategory
  processName?: string         // Process that triggered the threat
  processPid?: number
  details?: Record<string, string>
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

export const getThreatTypeLabel = (type: DetectedThreat['threatType']): string => {
  switch (type) {
    // Zer0 types
    case 'NONE': return 'None'
    case 'EXTENSION_MISMATCH': return 'Extension Mismatch'
    case 'HIDDEN_EXECUTABLE': return 'Hidden Executable'
    case 'HIGH_ENTROPY_TEXT': return 'Encrypted Text'
    case 'RANSOMWARE_PATTERN': return 'Ransomware Pattern'
    case 'MASS_MODIFICATION': return 'Mass Modification'
    case 'SUSPICIOUS_RENAME': return 'Suspicious Rename'
    case 'KNOWN_MALWARE_HASH': return 'Known Malware'
    case 'ANOMALOUS_BEHAVIOR': return 'Anomalous Behavior'
    case 'DOUBLE_EXTENSION': return 'Double Extension'
    case 'SCRIPT_IN_DATA': return 'Embedded Script'
    // Legacy types
    case 'RANSOMWARE': return 'Ransomware'
    case 'HIGH_ENTROPY': return 'High Entropy'
    case 'MASS_OPERATION': return 'Mass Operation'
    case 'SUSPICIOUS_PATTERN': return 'Suspicious Pattern'
    case 'UNKNOWN': return 'Unknown'
    default: return 'Unknown'
  }
}

export const getThreatLevelColor = (level: DetectedThreat['threatLevel']): string => {
  switch (level) {
    case 'CRITICAL': return 'text-red-600 dark:text-red-400'
    case 'HIGH': return 'text-orange-600 dark:text-orange-400'
    case 'MEDIUM': return 'text-yellow-600 dark:text-yellow-400'
    case 'LOW': return 'text-blue-600 dark:text-blue-400'
    case 'INFO': return 'text-cyan-600 dark:text-cyan-400'
    case 'NONE': return 'text-green-600 dark:text-green-400'
    default: return 'text-muted-foreground'
  }
}

export const getThreatLevelBgColor = (level: DetectedThreat['threatLevel']): string => {
  switch (level) {
    case 'CRITICAL': return 'bg-red-500/10 border-red-500/20'
    case 'HIGH': return 'bg-orange-500/10 border-orange-500/20'
    case 'MEDIUM': return 'bg-yellow-500/10 border-yellow-500/20'
    case 'LOW': return 'bg-blue-500/10 border-blue-500/20'
    case 'INFO': return 'bg-cyan-500/10 border-cyan-500/20'
    case 'NONE': return 'bg-green-500/10 border-green-500/20'
    default: return 'bg-muted/10 border-muted/20'
  }
}

// Zer0 specific helpers
export const getThreatTypeIcon = (type: DetectedThreat['threatType']): string => {
  switch (type) {
    case 'HIDDEN_EXECUTABLE': return '🎭'
    case 'DOUBLE_EXTENSION': return '📛'
    case 'RANSOMWARE_PATTERN': return '🔐'
    case 'EXTENSION_MISMATCH': return '⚠️'
    case 'HIGH_ENTROPY_TEXT': return '🔢'
    case 'SCRIPT_IN_DATA': return '💉'
    case 'MASS_MODIFICATION': return '🌊'
    case 'ANOMALOUS_BEHAVIOR': return '👁️'
    case 'KNOWN_MALWARE_HASH': return '☠️'
    case 'SUSPICIOUS_RENAME': return '📝'
    default: return '❓'
  }
}

export const getFileCategoryLabel = (category?: FileCategory): string => {
  if (!category) return 'Unknown'
  switch (category) {
    case 'TEXT': return 'Text'
    case 'DOCUMENT': return 'Document'
    case 'IMAGE': return 'Image'
    case 'VIDEO': return 'Video'
    case 'AUDIO': return 'Audio'
    case 'ARCHIVE': return 'Archive'
    case 'EXECUTABLE': return 'Executable'
    case 'DATABASE': return 'Database'
    case 'CONFIG': return 'Config'
    default: return 'Unknown'
  }
}
