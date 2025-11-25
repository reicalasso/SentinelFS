import { createContext, useContext, useState, ReactNode, useCallback } from 'react'
import { X, CheckCircle, AlertTriangle, Info, Users, FileText } from 'lucide-react'

export type NotificationType = 'success' | 'error' | 'warning' | 'info' | 'peer' | 'transfer'

export interface Notification {
  id: string
  type: NotificationType
  title: string
  message: string
  timestamp: Date
  autoClose?: boolean
}

interface NotificationContextType {
  notifications: Notification[]
  addNotification: (type: NotificationType, title: string, message: string, autoClose?: boolean) => void
  removeNotification: (id: string) => void
  clearAll: () => void
}

const NotificationContext = createContext<NotificationContextType | undefined>(undefined)

export function NotificationProvider({ children }: { children: ReactNode }) {
  const [notifications, setNotifications] = useState<Notification[]>([])

  const addNotification = useCallback((
    type: NotificationType,
    title: string,
    message: string,
    autoClose: boolean = true
  ) => {
    const id = `${Date.now()}-${Math.random().toString(36).substr(2, 9)}`
    const notification: Notification = { id, type, title, message, timestamp: new Date(), autoClose }
    
    setNotifications(prev => [...prev, notification])
    
    if (autoClose) {
      setTimeout(() => {
        setNotifications(prev => prev.filter(n => n.id !== id))
      }, 5000)
    }
  }, [])

  const removeNotification = useCallback((id: string) => {
    setNotifications(prev => prev.filter(n => n.id !== id))
  }, [])

  const clearAll = useCallback(() => {
    setNotifications([])
  }, [])

  return (
    <NotificationContext.Provider value={{ notifications, addNotification, removeNotification, clearAll }}>
      {children}
    </NotificationContext.Provider>
  )
}

export function useNotifications() {
  const context = useContext(NotificationContext)
  if (!context) {
    throw new Error('useNotifications must be used within a NotificationProvider')
  }
  return context
}

// Notification Display Component
export function NotificationToast() {
  const { notifications, removeNotification } = useNotifications()

  const getIcon = (type: NotificationType) => {
    switch (type) {
      case 'success': return <CheckCircle className="w-5 h-5 text-green-500" />
      case 'error': return <AlertTriangle className="w-5 h-5 text-red-500" />
      case 'warning': return <AlertTriangle className="w-5 h-5 text-yellow-500" />
      case 'peer': return <Users className="w-5 h-5 text-blue-500" />
      case 'transfer': return <FileText className="w-5 h-5 text-purple-500" />
      default: return <Info className="w-5 h-5 text-blue-500" />
    }
  }

  const getBgColor = (type: NotificationType) => {
    switch (type) {
      case 'success': return 'bg-green-500/10 border-green-500/20'
      case 'error': return 'bg-red-500/10 border-red-500/20'
      case 'warning': return 'bg-yellow-500/10 border-yellow-500/20'
      case 'peer': return 'bg-blue-500/10 border-blue-500/20'
      case 'transfer': return 'bg-purple-500/10 border-purple-500/20'
      default: return 'bg-blue-500/10 border-blue-500/20'
    }
  }

  if (notifications.length === 0) return null

  return (
    <div className="fixed bottom-4 right-4 z-50 space-y-2 max-w-sm">
      {notifications.slice(-5).map(notification => (
        <div
          key={notification.id}
          className={`p-4 rounded-lg border backdrop-blur-sm shadow-lg animate-in slide-in-from-right duration-300 ${getBgColor(notification.type)}`}
        >
          <div className="flex items-start gap-3">
            {getIcon(notification.type)}
            <div className="flex-1 min-w-0">
              <p className="font-medium text-sm">{notification.title}</p>
              <p className="text-xs text-muted-foreground mt-1">{notification.message}</p>
            </div>
            <button
              onClick={() => removeNotification(notification.id)}
              className="p-1 hover:bg-secondary rounded"
            >
              <X className="w-4 h-4 text-muted-foreground" />
            </button>
          </div>
        </div>
      ))}
    </div>
  )
}
