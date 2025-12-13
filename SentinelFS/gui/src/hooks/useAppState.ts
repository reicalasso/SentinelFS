import { useReducer, useCallback, useRef, useEffect } from 'react'

// Types
export interface AppMetrics {
  filesWatched?: number
  filesSynced?: number
  bytesUploaded?: number
  bytesDownloaded?: number
  peersConnected?: number
  syncErrors?: number
  [key: string]: any
}

export interface Peer {
  id: string
  ip: string
  port: number
  status: string
  latency: number
  lastSeen?: number
}

export interface SyncFile {
  path: string
  hash: string
  size: number
  timestamp: number
}

export interface ActivityItem {
  type: string
  path: string
  timestamp: number
}

export interface Transfer {
  id: string
  filename: string
  progress: number
  speed: number
  direction: 'upload' | 'download'
}

export interface TransferHistoryItem {
  file: string
  type: string
  time: string
}

export interface AppConfig {
  tcpPort?: number
  discoveryPort?: number
  watchDirectory?: string
  encryptionEnabled?: boolean
  uploadLimit?: number
  downloadLimit?: number
}

// Zer0 Threat Status type
export interface ThreatStatus {
  // Legacy ML format
  threatScore?: number
  threatLevel?: string
  totalThreats?: number
  ransomwareAlerts?: number
  highEntropyFiles?: number
  massOperationAlerts?: number
  avgFileEntropy?: number
  mlEnabled?: boolean
  // Zer0 format
  enabled?: boolean
  filesAnalyzed?: number
  threatsDetected?: number
  filesQuarantined?: number
  hiddenExecutables?: number
  extensionMismatches?: number
  ransomwarePatterns?: number
  behavioralAnomalies?: number
  magicByteValidation?: boolean
  behavioralAnalysis?: boolean
  fileTypeAwareness?: boolean
}

// File version type
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

// Conflict type
export interface Conflict {
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

// Detected Threat type
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

// State
export interface AppState {
  activeTab: 'dashboard' | 'files' | 'peers' | 'transfers' | 'falconstore' | 'settings' | 'logs'
  status: 'connected' | 'disconnected' | 'error'
  isPaused: boolean
  metrics: AppMetrics | null
  peers: Peer[]
  syncStatus: any
  files: SyncFile[]
  activity: ActivityItem[]
  transfers: Transfer[]
  transferHistory: TransferHistoryItem[]
  config: AppConfig | null
  logs: string[]
  toasts: string[]
  conflicts: Conflict[]
  showConflictModal: boolean
  versionedFiles: Map<string, FileVersion[]>
  threatStatus: ThreatStatus | null
  detectedThreats: DetectedThreat[]
  showQuarantineModal: boolean
}

// Actions
type AppAction =
  | { type: 'SET_TAB'; payload: AppState['activeTab'] }
  | { type: 'SET_STATUS'; payload: AppState['status'] }
  | { type: 'SET_PAUSED'; payload: boolean }
  | { type: 'SET_METRICS'; payload: AppMetrics }
  | { type: 'SET_PEERS'; payload: Peer[] }
  | { type: 'SET_SYNC_STATUS'; payload: any }
  | { type: 'SET_FILES'; payload: SyncFile[] }
  | { type: 'SET_ACTIVITY'; payload: ActivityItem[] }
  | { type: 'SET_TRANSFERS'; payload: { transfers: Transfer[], history: TransferHistoryItem[] } }
  | { type: 'SET_CONFIG'; payload: AppConfig }
  | { type: 'ADD_LOG'; payload: string }
  | { type: 'CLEAR_LOGS' }
  | { type: 'ADD_TOAST'; payload: string }
  | { type: 'REMOVE_TOAST'; payload: string }
  | { type: 'CLEAR_TOASTS' }
  | { type: 'SET_CONFLICTS'; payload: Conflict[] }
  | { type: 'SHOW_CONFLICT_MODAL'; payload: boolean }
  | { type: 'REMOVE_CONFLICT'; payload: number }
  | { type: 'SET_VERSIONED_FILES'; payload: Map<string, FileVersion[]> }
  | { type: 'DELETE_VERSION'; payload: { filePath: string; versionId: number } }
  | { type: 'SET_THREAT_STATUS'; payload: ThreatStatus }
  | { type: 'SET_DETECTED_THREATS'; payload: DetectedThreat[] }
  | { type: 'SHOW_QUARANTINE_MODAL'; payload: boolean }
  | { type: 'REMOVE_THREAT'; payload: number }
  | { type: 'MARK_THREAT_SAFE'; payload: number }

// Initial state
const initialState: AppState = {
  activeTab: 'dashboard',
  status: 'disconnected',
  isPaused: false,
  metrics: null,
  peers: [],
  syncStatus: null,
  files: [],
  activity: [],
  transfers: [],
  transferHistory: [],
  config: null,
  logs: [],
  toasts: [],
  conflicts: [],
  showConflictModal: false,
  versionedFiles: new Map(),
  threatStatus: null,
  detectedThreats: [],
  showQuarantineModal: false
}

// Reducer
function appReducer(state: AppState, action: AppAction): AppState {
  switch (action.type) {
    case 'SET_TAB':
      return { ...state, activeTab: action.payload }
    case 'SET_STATUS':
      return { ...state, status: action.payload }
    case 'SET_PAUSED':
      return { ...state, isPaused: action.payload }
    case 'SET_METRICS':
      return { ...state, metrics: action.payload }
    case 'SET_PEERS':
      return { ...state, peers: action.payload }
    case 'SET_SYNC_STATUS':
      return { ...state, syncStatus: action.payload }
    case 'SET_FILES':
      return { ...state, files: action.payload }
    case 'SET_ACTIVITY':
      return { ...state, activity: action.payload }
    case 'SET_TRANSFERS':
      return { ...state, transfers: action.payload.transfers, transferHistory: action.payload.history }
    case 'SET_CONFIG':
      return { ...state, config: action.payload }
    case 'ADD_LOG':
      return { ...state, logs: [...state.logs.slice(-99), action.payload] }
    case 'CLEAR_LOGS':
      return { ...state, logs: [] }
    case 'ADD_TOAST':
      return { ...state, toasts: [...state.toasts.slice(-4), action.payload] }
    case 'REMOVE_TOAST':
      return { ...state, toasts: state.toasts.filter(t => t !== action.payload) }
    case 'CLEAR_TOASTS':
      return { ...state, toasts: [] }
    case 'SET_CONFLICTS':
      return { ...state, conflicts: action.payload, showConflictModal: action.payload.length > 0 }
    case 'SHOW_CONFLICT_MODAL':
      return { ...state, showConflictModal: action.payload }
    case 'REMOVE_CONFLICT':
      return { ...state, conflicts: state.conflicts.filter(c => c.id !== action.payload) }
    case 'SET_VERSIONED_FILES':
      return { ...state, versionedFiles: action.payload }
    case 'DELETE_VERSION': {
      const newVersionedFiles = new Map(state.versionedFiles)
      const versions = newVersionedFiles.get(action.payload.filePath)
      if (versions) {
        const filtered = versions.filter(v => v.versionId !== action.payload.versionId)
        if (filtered.length > 0) {
          newVersionedFiles.set(action.payload.filePath, filtered)
        } else {
          newVersionedFiles.delete(action.payload.filePath)
        }
      }
      return { ...state, versionedFiles: newVersionedFiles }
    }
    case 'SET_THREAT_STATUS':
      return { ...state, threatStatus: action.payload }
    case 'SET_DETECTED_THREATS': {
      // Only auto-open modal if new threats are detected (count increased)
      const oldActiveThreatCount = state.detectedThreats.filter(t => !t.markedSafe).length
      const newActiveThreatCount = action.payload.filter((t: DetectedThreat) => !t.markedSafe).length
      const shouldAutoOpen = newActiveThreatCount > oldActiveThreatCount && newActiveThreatCount > 0
      
      return { 
        ...state, 
        detectedThreats: action.payload,
        // Only open if there are MORE active threats than before, not just if threats exist
        showQuarantineModal: state.showQuarantineModal || shouldAutoOpen
      }
    }
    case 'SHOW_QUARANTINE_MODAL':
      return { ...state, showQuarantineModal: action.payload }
    case 'REMOVE_THREAT':
      return { ...state, detectedThreats: state.detectedThreats.filter(t => t.id !== action.payload) }
    case 'MARK_THREAT_SAFE':
      return { 
        ...state, 
        detectedThreats: state.detectedThreats.map(t => 
          t.id === action.payload ? { ...t, markedSafe: true } : t
        ) 
      }
    default:
      return state
  }
}

// Hook
export function useAppState() {
  const [state, dispatch] = useReducer(appReducer, initialState)
  const lastLogRef = useRef<string | null>(null)
  const lastPeerCountRef = useRef<number>(0)

  // Actions
  const setTab = useCallback((tab: AppState['activeTab']) => {
    dispatch({ type: 'SET_TAB', payload: tab })
  }, [])

  const setStatus = useCallback((status: AppState['status']) => {
    dispatch({ type: 'SET_STATUS', payload: status })
  }, [])

  const setPaused = useCallback((paused: boolean) => {
    dispatch({ type: 'SET_PAUSED', payload: paused })
  }, [])

  const handleData = useCallback((data: any) => {
    if (data.type === 'METRICS') {
      dispatch({ type: 'SET_METRICS', payload: data.payload })
    } else if (data.type === 'PEERS') {
      if (data.payload.length > 0) {
        dispatch({ type: 'SET_PEERS', payload: data.payload })
        lastPeerCountRef.current = data.payload.length
      } else if (lastPeerCountRef.current === 0) {
        dispatch({ type: 'SET_PEERS', payload: [] })
      }
    } else if (data.type === 'STATUS') {
      dispatch({ type: 'SET_SYNC_STATUS', payload: data.payload })
    } else if (data.type === 'FILES') {
      dispatch({ type: 'SET_FILES', payload: data.payload })
    } else if (data.type === 'ACTIVITY') {
      dispatch({ type: 'SET_ACTIVITY', payload: data.payload })
    } else if (data.type === 'TRANSFERS') {
      dispatch({ type: 'SET_TRANSFERS', payload: { transfers: data.payload, history: data.history || [] } })
    } else if (data.type === 'CONFIG') {
      dispatch({ type: 'SET_CONFIG', payload: data.payload })
    } else if (data.type === 'THREAT_STATUS') {
      dispatch({ type: 'SET_THREAT_STATUS', payload: data.payload })
    } else if (data.type === 'DETECTED_THREATS') {
      dispatch({ type: 'SET_DETECTED_THREATS', payload: data.payload })
    }
  }, [])

  const handleLog = useCallback((log: string) => {
    // Always strip ANSI codes for clean UI display
    const cleanLog = log.replace(/\u001b\[[0-9;]*m/g, '')
    const hasTimestamp = /^\[\d{4}-\d{2}-\d{2}/.test(cleanLog)
    // Use cleanLog here instead of original log
    const formattedLog = hasTimestamp ? cleanLog : `[${new Date().toLocaleTimeString()}] ${cleanLog}`
    
    if (lastLogRef.current === formattedLog) return
    lastLogRef.current = formattedLog
    dispatch({ type: 'ADD_LOG', payload: formattedLog })
  }, [])

  const clearLogs = useCallback(() => {
    dispatch({ type: 'CLEAR_LOGS' })
  }, [])

  const addToast = useCallback((message: string) => {
    dispatch({ type: 'ADD_TOAST', payload: message })
    setTimeout(() => {
      dispatch({ type: 'REMOVE_TOAST', payload: message })
    }, 5000)
  }, [])

  const clearToasts = useCallback(() => {
    dispatch({ type: 'CLEAR_TOASTS' })
  }, [])

  const setConflicts = useCallback((conflicts: Conflict[]) => {
    dispatch({ type: 'SET_CONFLICTS', payload: conflicts })
  }, [])

  const showConflictModal = useCallback((show: boolean) => {
    dispatch({ type: 'SHOW_CONFLICT_MODAL', payload: show })
  }, [])

  const resolveConflict = useCallback((conflictId: number) => {
    dispatch({ type: 'REMOVE_CONFLICT', payload: conflictId })
  }, [])

  const setVersionedFiles = useCallback((files: Map<string, FileVersion[]>) => {
    dispatch({ type: 'SET_VERSIONED_FILES', payload: files })
  }, [])

  const deleteVersion = useCallback((filePath: string, versionId: number) => {
    dispatch({ type: 'DELETE_VERSION', payload: { filePath, versionId } })
  }, [])

  const setDetectedThreats = useCallback((threats: DetectedThreat[]) => {
    dispatch({ type: 'SET_DETECTED_THREATS', payload: threats })
  }, [])

  const showQuarantineModal = useCallback((show: boolean) => {
    dispatch({ type: 'SHOW_QUARANTINE_MODAL', payload: show })
  }, [])

  const removeThreat = useCallback((threatId: number) => {
    dispatch({ type: 'REMOVE_THREAT', payload: threatId })
  }, [])

  const markThreatSafe = useCallback((threatId: number) => {
    dispatch({ type: 'MARK_THREAT_SAFE', payload: threatId })
  }, [])

  return {
    state,
    actions: {
      setTab,
      setStatus,
      setPaused,
      handleData,
      handleLog,
      clearLogs,
      addToast,
      clearToasts,
      setConflicts,
      showConflictModal,
      resolveConflict,
      setVersionedFiles,
      deleteVersion,
      setDetectedThreats,
      showQuarantineModal,
      removeThreat,
      markThreatSafe
    }
  }
}
