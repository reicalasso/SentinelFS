import { useReducer, useCallback } from 'react'

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
  versions?: any[]
}

interface ConflictsState {
  conflicts: Conflict[]
  showConflictModal: boolean
}

type ConflictsAction =
  | { type: 'SET_CONFLICTS'; payload: Conflict[] }
  | { type: 'SHOW_CONFLICT_MODAL'; payload: boolean }
  | { type: 'REMOVE_CONFLICT'; payload: number }

const initialState: ConflictsState = {
  conflicts: [],
  showConflictModal: false
}

function conflictsReducer(state: ConflictsState, action: ConflictsAction): ConflictsState {
  switch (action.type) {
    case 'SET_CONFLICTS':
      return { 
        ...state, 
        conflicts: action.payload, 
        showConflictModal: action.payload.length > 0 
      }
    case 'SHOW_CONFLICT_MODAL':
      return { ...state, showConflictModal: action.payload }
    case 'REMOVE_CONFLICT':
      return { ...state, conflicts: state.conflicts.filter(c => c.id !== action.payload) }
    default:
      return state
  }
}

export function useConflictsState() {
  const [state, dispatch] = useReducer(conflictsReducer, initialState)

  const setConflicts = useCallback((conflicts: Conflict[]) => {
    dispatch({ type: 'SET_CONFLICTS', payload: conflicts })
  }, [])

  const setConflictModalOpen = useCallback((show: boolean) => {
    dispatch({ type: 'SHOW_CONFLICT_MODAL', payload: show })
  }, [])

  const resolveConflict = useCallback((conflictId: number) => {
    dispatch({ type: 'REMOVE_CONFLICT', payload: conflictId })
  }, [])

  return {
    conflicts: state.conflicts,
    isConflictModalOpen: state.showConflictModal,
    setConflicts,
    setConflictModalOpen,
    resolveConflict
  }
}
