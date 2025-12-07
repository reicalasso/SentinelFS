import { X } from 'lucide-react'

interface ToastListProps {
  toasts: string[]
  onClear?: () => void
}

export function ToastList({ toasts, onClear }: ToastListProps) {
  if (toasts.length === 0) {
    return null
  }

  return (
    <div className="toast-container">
      {toasts.map((toast, index) => (
        <div key={`${toast}-${index}`} className="toast-item">
          <p className="toast-message">{toast}</p>
          {onClear && (
            <button
              onClick={onClear}
              className="toast-dismiss-btn"
              aria-label="Dismiss toast"
            >
              <X className="w-4 h-4" />
            </button>
          )}
        </div>
      ))}
    </div>
  )
}
