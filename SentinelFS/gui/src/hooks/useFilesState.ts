import { useReducer, useCallback } from 'react'

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

interface FilesState {
  files: SyncFile[]
  activity: ActivityItem[]
  versionedFiles: Map<string, FileVersion[]>
}

type FilesAction =
  | { type: 'SET_FILES'; payload: SyncFile[] }
  | { type: 'SET_ACTIVITY'; payload: ActivityItem[] }
  | { type: 'SET_VERSIONED_FILES'; payload: Map<string, FileVersion[]> }
  | { type: 'DELETE_VERSION'; payload: { filePath: string; versionId: number } }

const initialState: FilesState = {
  files: [],
  activity: [],
  versionedFiles: new Map()
}

function filesReducer(state: FilesState, action: FilesAction): FilesState {
  switch (action.type) {
    case 'SET_FILES':
      return { ...state, files: action.payload }
    case 'SET_ACTIVITY':
      return { ...state, activity: action.payload }
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
    default:
      return state
  }
}

export function useFilesState() {
  const [state, dispatch] = useReducer(filesReducer, initialState)

  const setFiles = useCallback((files: SyncFile[]) => {
    dispatch({ type: 'SET_FILES', payload: files })
  }, [])

  const setActivity = useCallback((activity: ActivityItem[]) => {
    dispatch({ type: 'SET_ACTIVITY', payload: activity })
  }, [])

  const setVersionedFiles = useCallback((files: Map<string, FileVersion[]>) => {
    dispatch({ type: 'SET_VERSIONED_FILES', payload: files })
  }, [])

  const deleteVersion = useCallback((filePath: string, versionId: number) => {
    dispatch({ type: 'DELETE_VERSION', payload: { filePath, versionId } })
  }, [])

  return {
    files: state.files,
    activity: state.activity,
    versionedFiles: state.versionedFiles,
    setFiles,
    setActivity,
    setVersionedFiles,
    deleteVersion
  }
}
