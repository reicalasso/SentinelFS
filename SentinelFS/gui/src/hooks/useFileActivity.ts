import { useMemo } from 'react'

interface ActivityItem {
  type: string
  file?: string
  time?: string
  details?: string
  timestamp?: number
}

interface FileActivityData {
  filesSynced: number
  filesUploaded: number
  filesDownloaded: number
  totalFileSize: number
  pendingFiles: number
  activeTransfers: number
  activityHistory: Array<{
    time: string
    filesUploaded: number
    filesDownloaded: number
  }>
}

export function useFileActivity(activity: ActivityItem[]): FileActivityData {
  const fileActivityData = useMemo(() => {
    // Initialize counters
    let synced = 0
    let uploaded = 0
    let downloaded = 0
    let active = 0
    const timeBuckets = new Map<string, { uploads: number; downloads: number }>()

    // Process activity items
    activity.forEach((item) => {
      // Create time bucket (5-second intervals)
      const now = new Date()
      const timestamp = item.timestamp || now.getTime()
      const date = new Date(timestamp)
      
      // Round down to nearest 5 seconds
      const seconds = Math.floor(date.getSeconds() / 5) * 5
      date.setSeconds(seconds, 0)
      
      const timeStr = date.toLocaleTimeString()
      
      // Initialize time bucket if not exists
      if (!timeBuckets.has(timeStr)) {
        timeBuckets.set(timeStr, { uploads: 0, downloads: 0 })
      }

      // Parse activity type and update counters
      if (item.details?.includes('synced')) {
        synced++
        // Assume sync is both upload and download
        uploaded++
        downloaded++
        timeBuckets.get(timeStr)!.uploads++
        timeBuckets.get(timeStr)!.downloads++
      } else if (item.details?.includes('uploaded')) {
        uploaded++
        timeBuckets.get(timeStr)!.uploads++
      } else if (item.details?.includes('downloaded')) {
        downloaded++
        timeBuckets.get(timeStr)!.downloads++
      } else if (item.details?.includes('transferring') || item.details?.includes('in progress')) {
        active++
      }
    })

    // Create activity history array (last 30 entries)
    const history = Array.from(timeBuckets.entries())
      .slice(-30)
      .map(([time, data]) => ({
        time,
        filesUploaded: data.uploads,
        filesDownloaded: data.downloads
      }))

    // Calculate pending files (mock for now)
    const pendingFiles = Math.max(0, synced - uploaded - downloaded)

    return {
      filesSynced: synced,
      filesUploaded: uploaded,
      filesDownloaded: downloaded,
      totalFileSize: 0, // Set to 0 since we don't have real size data
      pendingFiles,
      activeTransfers: active,
      activityHistory: history
    }
  }, [activity])

  return fileActivityData
}
