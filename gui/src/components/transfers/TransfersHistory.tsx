import { ArrowDown, ArrowUp, X, Clock, CheckCircle, FileText, RefreshCw } from 'lucide-react'

interface HistoryItem {
  file: string
  type: string
  time: string
  size?: string
}

interface TransfersHistoryProps {
  history: HistoryItem[]
}

export function TransfersHistory({ history }: TransfersHistoryProps) {
  if (!history || history.length === 0) {
    return (
      <div className="transfers-history-empty-modern">
        <FileText className="w-5 h-5 opacity-40" />
        <p>No recent transfer history</p>
      </div>
    )
  }

  const getTypeInfo = (type: string) => {
    const lowerType = type?.toLowerCase() || ''
    if (lowerType === 'download' || lowerType === 'pull') {
      return { icon: ArrowDown, class: 'transfers-history-icon-download', badge: 'badge-download', label: 'download' }
    }
    if (lowerType === 'delete' || lowerType === 'removed') {
      return { icon: X, class: 'transfers-history-icon-delete', badge: 'badge-delete', label: 'delete' }
    }
    if (lowerType === 'sync' || lowerType === 'synced') {
      return { icon: RefreshCw, class: 'transfers-history-icon-upload', badge: 'badge-upload', label: 'sync' }
    }
    // Default to upload
    return { icon: ArrowUp, class: 'transfers-history-icon-upload', badge: 'badge-upload', label: type || 'upload' }
  }

  return (
    <div className="transfers-history-modern max-h-[400px] overflow-y-auto">
      {history.map((item: HistoryItem, i: number) => {
        const typeInfo = getTypeInfo(item.type)
        const Icon = typeInfo.icon
        
        return (
          <div key={i} className="transfers-history-item-modern">
            <div className="transfers-history-item-left">
              <div className={`transfers-history-icon-modern ${typeInfo.class}`}>
                <Icon className="w-4 h-4" />
              </div>
              <div className="transfers-history-info">
                <div className="transfers-history-filename-modern">{item.file}</div>
                <div className="transfers-history-meta">
                  <span className={`transfers-history-type-badge ${typeInfo.badge}`}>
                    {typeInfo.label}
                  </span>
                  {item.size && <span className="transfers-history-size">{item.size}</span>}
                </div>
              </div>
            </div>
            <div className="transfers-history-item-right">
              <div className="transfers-history-status">
                <CheckCircle className="w-3.5 h-3.5 text-success" />
              </div>
              <div className="transfers-history-time-modern">
                <Clock className="w-3 h-3" />
                <span>{item.time || 'Just now'}</span>
              </div>
            </div>
          </div>
        )
      })}
    </div>
  )
}
