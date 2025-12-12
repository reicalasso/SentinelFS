import { useReducer, useCallback } from 'react'

export interface ThreatStatus {
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
  // Legacy ML format
  threatScore?: number
  threatLevel?: string
  totalThreats?: number
  ransomwareAlerts?: number
  highEntropyFiles?: number
  massOperationAlerts?: number
  avgFileEntropy?: number
  mlEnabled?: boolean
}

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

interface ThreatsState {
  threatStatus: ThreatStatus | null
  detectedThreats: DetectedThreat[]
  showQuarantineModal: boolean
}

type ThreatsAction =
  | { type: 'SET_THREAT_STATUS'; payload: ThreatStatus }
  | { type: 'SET_DETECTED_THREATS'; payload: DetectedThreat[] }
  | { type: 'SHOW_QUARANTINE_MODAL'; payload: boolean }
  | { type: 'REMOVE_THREAT'; payload: number }
  | { type: 'MARK_THREAT_SAFE'; payload: number }

const initialState: ThreatsState = {
  threatStatus: null,
  detectedThreats: [],
  showQuarantineModal: false
}

function threatsReducer(state: ThreatsState, action: ThreatsAction): ThreatsState {
  switch (action.type) {
    case 'SET_THREAT_STATUS':
      return { ...state, threatStatus: action.payload }
    case 'SET_DETECTED_THREATS': {
      const oldActiveThreatCount = state.detectedThreats.filter(t => !t.markedSafe).length
      const newActiveThreatCount = action.payload.filter((t: DetectedThreat) => !t.markedSafe).length
      const shouldAutoOpen = newActiveThreatCount > oldActiveThreatCount && newActiveThreatCount > 0
      
      return { 
        ...state, 
        detectedThreats: action.payload,
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

export function useThreatsState() {
  const [state, dispatch] = useReducer(threatsReducer, initialState)

  const setThreatStatus = useCallback((status: ThreatStatus) => {
    dispatch({ type: 'SET_THREAT_STATUS', payload: status })
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
    threatStatus: state.threatStatus,
    detectedThreats: state.detectedThreats,
    isQuarantineModalOpen: state.showQuarantineModal,
    setThreatStatus,
    setDetectedThreats,
    setQuarantineModalOpen: showQuarantineModal,
    removeThreat,
    markThreatSafe
  }
}
