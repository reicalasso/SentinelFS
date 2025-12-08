import { Trash2, ShieldCheck, ShieldAlert } from 'lucide-react'
import { DetectedThreat } from './types'

interface ThreatActionsProps {
  threat: DetectedThreat | null
  onDeleteThreat: (threatId: number) => void
  onMarkSafe: (threatId: number) => void
}

export function ThreatActions({ threat, onDeleteThreat, onMarkSafe }: ThreatActionsProps) {
  if (!threat) {
    return null
  }

  return (
    <div className="p-4 border-t border-border bg-card/30">
      <div className="flex gap-3">
        {!threat.markedSafe ? (
          <>
            <button
              onClick={() => onMarkSafe(threat.id)}
              className="flex-1 flex items-center justify-center gap-2 px-4 py-2.5 bg-green-500/10 hover:bg-green-500/20 text-green-600 dark:text-green-400 rounded-lg border border-green-500/20 hover:border-green-500/30 transition-all font-medium"
            >
              <ShieldCheck className="w-4 h-4" />
              Mark as Safe
            </button>
            <button
              onClick={() => {
                if (confirm(`Are you sure you want to permanently delete "${threat.filePath}"?`)) {
                  onDeleteThreat(threat.id)
                }
              }}
              className="flex-1 flex items-center justify-center gap-2 px-4 py-2.5 bg-red-500/10 hover:bg-red-500/20 text-red-600 dark:text-red-400 rounded-lg border border-red-500/20 hover:border-red-500/30 transition-all font-medium"
            >
              <Trash2 className="w-4 h-4" />
              Delete
            </button>
          </>
        ) : (
          <>
            <button
              onClick={() => onMarkSafe(threat.id)}
              className="flex-1 flex items-center justify-center gap-2 px-4 py-2.5 bg-orange-500/10 hover:bg-orange-500/20 text-orange-600 dark:text-orange-400 rounded-lg border border-orange-500/20 hover:border-orange-500/30 transition-all font-medium"
            >
              <ShieldAlert className="w-4 h-4" />
              Unmark as Safe
            </button>
            <button
              onClick={() => {
                if (confirm(`Are you sure you want to permanently delete "${threat.filePath}"?`)) {
                  onDeleteThreat(threat.id)
                }
              }}
              className="flex-1 flex items-center justify-center gap-2 px-4 py-2.5 bg-red-500/10 hover:bg-red-500/20 text-red-600 dark:text-red-400 rounded-lg border border-red-500/20 hover:border-red-500/30 transition-all font-medium"
            >
              <Trash2 className="w-4 h-4" />
              Delete
            </button>
          </>
        )}
      </div>
    </div>
  )
}
