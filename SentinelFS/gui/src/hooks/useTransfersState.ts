import { useReducer, useCallback } from 'react'

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

interface TransfersState {
  transfers: Transfer[]
  transferHistory: TransferHistoryItem[]
}

type TransfersAction =
  | { type: 'SET_TRANSFERS'; payload: { transfers: Transfer[], history: TransferHistoryItem[] } }
  | { type: 'UPDATE_TRANSFER'; payload: Transfer }
  | { type: 'REMOVE_TRANSFER'; payload: string }

const initialState: TransfersState = {
  transfers: [],
  transferHistory: []
}

function transfersReducer(state: TransfersState, action: TransfersAction): TransfersState {
  switch (action.type) {
    case 'SET_TRANSFERS':
      return { 
        ...state, 
        transfers: action.payload.transfers, 
        transferHistory: action.payload.history 
      }
    case 'UPDATE_TRANSFER':
      return {
        ...state,
        transfers: state.transfers.map(t => 
          t.id === action.payload.id ? action.payload : t
        )
      }
    case 'REMOVE_TRANSFER':
      return {
        ...state,
        transfers: state.transfers.filter(t => t.id !== action.payload)
      }
    default:
      return state
  }
}

export function useTransfersState() {
  const [state, dispatch] = useReducer(transfersReducer, initialState)

  const setTransfers = useCallback((transfers: Transfer[], history: TransferHistoryItem[] = []) => {
    dispatch({ type: 'SET_TRANSFERS', payload: { transfers, history } })
  }, [])

  const updateTransfer = useCallback((transfer: Transfer) => {
    dispatch({ type: 'UPDATE_TRANSFER', payload: transfer })
  }, [])

  const removeTransfer = useCallback((transferId: string) => {
    dispatch({ type: 'REMOVE_TRANSFER', payload: transferId })
  }, [])

  return {
    transfers: state.transfers,
    transferHistory: state.transferHistory,
    setTransfers,
    updateTransfer,
    removeTransfer
  }
}
