import { AlertTriangle, FileWarning, Shield, CheckCircle2 } from 'lucide-react'
import { DetectedThreat, getFileName, formatRelativeTime, getThreatTypeLabel, getThreatLevelColor, getThreatLevelBgColor } from './types'

interface ThreatListProps {
  threats: DetectedThreat[]
  selectedThreat: DetectedThreat | null
  onSelectThreat: (threat: DetectedThreat) => void
  selectedThreats?: Set<number>
  onToggleSelect?: (threatId: number) => void
  onToggleSelectAll?: () => void
  multiSelectMode?: boolean
}

export function ThreatList({ 
  threats, 
  selectedThreat, 
  onSelectThreat, 
  selectedThreats = new Set(),
  onToggleSelect,
  onToggleSelectAll,
  multiSelectMode = false
}: ThreatListProps) {
  const allSelected = threats.length > 0 && threats.every(t => selectedThreats.has(t.id))
  const someSelected = threats.some(t => selectedThreats.has(t.id)) && !allSelected

  if (threats.length === 0) {
    return (
      <div className="flex flex-col items-center justify-center h-full text-center p-8">
        <Shield className="w-16 h-16 text-green-500 mb-4" />
        <h3 className="text-lg font-semibold mb-2">No Threats</h3>
        <p className="text-sm text-muted-foreground">
          There are no threats currently in quarantine.
        </p>
      </div>
    )
  }

  const getThreatIcon = (threat: DetectedThreat) => {
    if (threat.markedSafe) {
      return <CheckCircle2 className="w-5 h-5 text-green-500" />
    }
    switch (threat.threatLevel) {
      case 'CRITICAL':
      case 'HIGH':
        return <AlertTriangle className="w-5 h-5 text-red-500" />
      case 'MEDIUM':
        return <FileWarning className="w-5 h-5 text-yellow-500" />
      default:
        return <FileWarning className="w-5 h-5 text-blue-500" />
    }
  }

  return (
    <div className="space-y-2 overflow-y-auto h-full custom-scrollbar">
      {/* Select All Header */}
      {multiSelectMode && threats.length > 0 && (
        <div className="sticky top-0 bg-background/95 backdrop-blur-sm border-b border-border p-3 -mx-2 mb-2">
          <label className="flex items-center gap-2 cursor-pointer">
            <input
              type="checkbox"
              checked={allSelected}
              ref={input => {
                if (input) input.indeterminate = someSelected
              }}
              onChange={onToggleSelectAll}
              className="w-4 h-4 rounded border-border"
            />
            <span className="text-sm text-muted-foreground">
              {allSelected ? 'Deselect All' : someSelected ? `${selectedThreats.size} Selected` : 'Select All'}
            </span>
          </label>
        </div>
      )}
      
      {threats.map((threat) => (
        <div
          key={threat.id}
          onClick={() => !multiSelectMode && onSelectThreat(threat)}
          className={`
            p-4 rounded-lg border cursor-pointer transition-all
            ${selectedThreat?.id === threat.id && !multiSelectMode
              ? 'bg-primary/10 border-primary shadow-sm'
              : 'bg-card/50 border-border hover:bg-card hover:border-border/60'
            }
            ${selectedThreats.has(threat.id) ? 'ring-2 ring-primary ring-offset-2 ring-offset-background' : ''}
            ${threat.markedSafe ? 'opacity-60' : ''}
          `}
        >
          <div className="flex items-start gap-3">
            {multiSelectMode && onToggleSelect && (
              <input
                type="checkbox"
                checked={selectedThreats.has(threat.id)}
                onChange={() => onToggleSelect(threat.id)}
                onClick={(e) => e.stopPropagation()}
                className="w-4 h-4 mt-1 rounded border-border"
              />
            )}
            {getThreatIcon(threat)}
            <div className="flex-1 min-w-0">
              <div className="flex items-center gap-2 mb-1">
                <span className="font-medium truncate">{getFileName(threat.filePath)}</span>
                {threat.markedSafe && (
                  <span className="px-2 py-0.5 text-xs bg-green-500/10 text-green-600 dark:text-green-400 rounded-full border border-green-500/20">
                    Safe
                  </span>
                )}
              </div>
              <p className="text-xs text-muted-foreground truncate mb-2">
                {threat.filePath}
              </p>
              <div className="flex flex-wrap items-center gap-2">
                <span className={`px-2 py-0.5 text-xs rounded-full border ${getThreatLevelBgColor(threat.threatLevel)} ${getThreatLevelColor(threat.threatLevel)}`}>
                  {threat.threatLevel}
                </span>
                <span className="px-2 py-0.5 text-xs bg-background/50 rounded-full border border-border">
                  {getThreatTypeLabel(threat.threatType)}
                </span>
                <span className="text-xs text-muted-foreground">
                  {formatRelativeTime(threat.detectedAt)}
                </span>
              </div>
            </div>
          </div>
        </div>
      ))}
    </div>
  )
}
