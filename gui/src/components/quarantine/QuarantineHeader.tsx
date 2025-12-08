import { Shield, X } from 'lucide-react'

interface QuarantineHeaderProps {
  threatCount: number
  onClose: () => void
}

export function QuarantineHeader({ threatCount, onClose }: QuarantineHeaderProps) {
  return (
    <div className="flex items-center justify-between p-6 border-b border-border bg-gradient-to-r from-red-500/5 to-orange-500/5">
      <div className="flex items-center gap-4">
        <div className="p-3 bg-red-500/10 rounded-xl">
          <Shield className="w-6 h-6 text-red-500" />
        </div>
        <div>
          <h2 className="text-2xl font-bold">Threat Quarantine Center</h2>
          <p className="text-sm text-muted-foreground mt-1">
            {threatCount > 0 
              ? `${threatCount} detected threat${threatCount > 1 ? 's' : ''}` 
              : 'No threats detected'
            }
          </p>
        </div>
      </div>
      <button
        onClick={onClose}
        className="p-2 hover:bg-background/80 rounded-lg transition-colors"
        aria-label="Close"
      >
        <X className="w-5 h-5" />
      </button>
    </div>
  )
}
