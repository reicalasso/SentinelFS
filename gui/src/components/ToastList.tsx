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
    <div className="absolute right-2 sm:right-6 top-2 sm:top-6 w-[calc(100%-1rem)] sm:w-80 space-y-2 z-50">
      {toasts.map((toast, index) => (
        <div
          key={`${toast}-${index}`}
          className="bg-slate-900/90 border border-border rounded-lg sm:rounded-xl px-3 sm:px-4 py-2 sm:py-3 text-sm text-white flex items-start justify-between gap-2 sm:gap-3 shadow-lg"
        >
          <p className="break-words text-xs leading-snug">{toast}</p>
          {onClear && (
            <button
              onClick={onClear}
              className="text-muted-foreground hover:text-foreground"
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
