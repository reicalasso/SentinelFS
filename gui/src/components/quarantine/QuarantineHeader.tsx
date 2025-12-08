import { Shield, X, CheckSquare } from 'lucide-react'

interface QuarantineHeaderProps {
  threatCount: number
  onClose: () => void
  multiSelectMode?: boolean
  onToggleMultiSelect?: () => void
  selectedCount?: number
}

export function QuarantineHeader({ 
  threatCount, 
  onClose, 
  multiSelectMode = false,
  onToggleMultiSelect,
  selectedCount = 0
}: QuarantineHeaderProps) {
  return (
    <div className="flex items-center justify-between p-6 border-b border-border bg-gradient-to-r from-red-500/5 to-orange-500/5">
      <div className="flex items-center gap-4">
        <div className="p-3 bg-red-500/10 rounded-xl">
          <Shield className="w-6 h-6 text-red-500" />
        </div>
        <div>
          <h2 className="text-2xl font-bold">Threat Quarantine Center</h2>
          <p className="text-sm text-muted-foreground mt-1">
            {multiSelectMode && selectedCount > 0 
              ? `${selectedCount} threat${selectedCount > 1 ? 's' : ''} selected`
              : threatCount > 0 
                ? `${threatCount} detected threat${threatCount > 1 ? 's' : ''}` 
                : 'No threats detected'
            }
          </p>
        </div>
      </div>
      <div className="flex items-center gap-2">
        {onToggleMultiSelect && (
          <button
            onClick={onToggleMultiSelect}
            className={`flex items-center gap-2 px-3 py-2 rounded-lg transition-colors ${
              multiSelectMode 
                ? 'bg-primary text-primary-foreground' 
                : 'hover:bg-background/80'
            }`}
            aria-label="Toggle multi-select"
          >
            <CheckSquare className="w-4 h-4" />
            <span className="text-sm font-medium">Multi-Select</span>
          </button>
        )}
        <button
          onClick={onClose}
          className="p-2 hover:bg-background/80 rounded-lg transition-colors"
          aria-label="Close"
        >
          <X className="w-5 h-5" />
        </button>
      </div>
    </div>
  )
}
