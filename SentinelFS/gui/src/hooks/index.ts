/**
 * State management hooks - split by domain for better performance
 * 
 * Each hook manages a specific domain of state, reducing unnecessary
 * re-renders when unrelated state changes.
 * 
 * Usage:
 * ```tsx
 * // Instead of one large useAppState hook:
 * const { state, actions } = useAppState()
 * 
 * // Use domain-specific hooks:
 * const { peers, setPeers } = usePeersState()
 * const { files, setFiles } = useFilesState()
 * const { transfers, setTransfers } = useTransfersState()
 * const { threatStatus, setThreatStatus } = useThreatsState()
 * const { conflicts, setConflicts } = useConflictsState()
 * ```
 */

export { usePeersState, type Peer } from './usePeersState'
export { useFilesState, type SyncFile, type ActivityItem, type FileVersion } from './useFilesState'
export { useTransfersState, type Transfer, type TransferHistoryItem } from './useTransfersState'
export { useThreatsState, type ThreatStatus, type DetectedThreat } from './useThreatsState'
export { useConflictsState, type Conflict } from './useConflictsState'

// Re-export legacy hook for backward compatibility
export { useAppState } from './useAppState'
