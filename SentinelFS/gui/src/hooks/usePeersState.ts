import { useReducer, useCallback, useRef } from 'react'

export interface Peer {
  id: string
  ip: string
  port: number
  status: string
  latency: number
  lastSeen: number
}

interface PeersState {
  peers: Peer[]
}

type PeersAction =
  | { type: 'SET_PEERS'; payload: Peer[] }
  | { type: 'UPDATE_PEER'; payload: Peer }
  | { type: 'REMOVE_PEER'; payload: string }

const initialState: PeersState = {
  peers: []
}

function peersReducer(state: PeersState, action: PeersAction): PeersState {
  switch (action.type) {
    case 'SET_PEERS':
      return { ...state, peers: action.payload }
    case 'UPDATE_PEER':
      return {
        ...state,
        peers: state.peers.map(p => 
          p.id === action.payload.id ? action.payload : p
        )
      }
    case 'REMOVE_PEER':
      return {
        ...state,
        peers: state.peers.filter(p => p.id !== action.payload)
      }
    default:
      return state
  }
}

export function usePeersState() {
  const [state, dispatch] = useReducer(peersReducer, initialState)
  const lastPeerCountRef = useRef<number>(0)

  const setPeers = useCallback((peers: Peer[]) => {
    if (peers.length > 0) {
      dispatch({ type: 'SET_PEERS', payload: peers })
      lastPeerCountRef.current = peers.length
    } else if (lastPeerCountRef.current === 0) {
      dispatch({ type: 'SET_PEERS', payload: [] })
    }
  }, [])

  const updatePeer = useCallback((peer: Peer) => {
    dispatch({ type: 'UPDATE_PEER', payload: peer })
  }, [])

  const removePeer = useCallback((peerId: string) => {
    dispatch({ type: 'REMOVE_PEER', payload: peerId })
  }, [])

  return {
    peers: state.peers,
    setPeers,
    updatePeer,
    removePeer
  }
}
