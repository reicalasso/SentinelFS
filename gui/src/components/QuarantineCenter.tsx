import { useState, useMemo } from 'react'
import { 
  QuarantineHeader, 
  QuarantineSearchBar, 
  ThreatList, 
  ThreatDetails, 
  ThreatActions,
  DetectedThreat,
  ViewMode,
  SortBy
} from './quarantine'

// Re-export types for external use
export type { DetectedThreat }

interface QuarantineCenterProps {
  threats: DetectedThreat[]
  isOpen: boolean
  onClose: () => void
  onDeleteThreat: (threatId: number) => void
  onMarkSafe: (threatId: number) => void
}

export function QuarantineCenter({ 
  threats, 
  isOpen, 
  onClose, 
  onDeleteThreat,
  onMarkSafe
}: QuarantineCenterProps) {
  const [selectedThreat, setSelectedThreat] = useState<DetectedThreat | null>(
    threats.length > 0 ? threats[0] : null
  )
  const [selectedThreats, setSelectedThreats] = useState<Set<number>>(new Set())
  const [multiSelectMode, setMultiSelectMode] = useState(false)
  const [viewMode, setViewMode] = useState<ViewMode>('all')
  const [searchQuery, setSearchQuery] = useState('')
  const [sortBy, setSortBy] = useState<SortBy>('time')

  // Multi-select handlers
  const handleToggleSelect = (threatId: number) => {
    setSelectedThreats(prev => {
      const newSet = new Set(prev)
      if (newSet.has(threatId)) {
        newSet.delete(threatId)
      } else {
        newSet.add(threatId)
      }
      return newSet
    })
  }

  const handleToggleSelectAll = () => {
    if (filteredThreats.length === selectedThreats.size) {
      setSelectedThreats(new Set())
    } else {
      setSelectedThreats(new Set(filteredThreats.map(t => t.id)))
    }
  }

  const handleBulkDelete = async () => {
    const promises = Array.from(selectedThreats).map(threatId => onDeleteThreat(threatId))
    await Promise.all(promises)
    setSelectedThreats(new Set())
  }

  const handleBulkMarkSafe = async () => {
    const promises = Array.from(selectedThreats).map(threatId => onMarkSafe(threatId))
    await Promise.all(promises)
    setSelectedThreats(new Set())
  }

  // Filter and sort threats
  const filteredThreats = useMemo(() => {
    let filtered = threats.filter(t => {
      const matchesSearch = t.filePath.toLowerCase().includes(searchQuery.toLowerCase())
      
      if (viewMode === 'active') {
        return matchesSearch && !t.markedSafe
      } else if (viewMode === 'safe') {
        return matchesSearch && t.markedSafe
      }
      return matchesSearch
    })
    
    switch (sortBy) {
      case 'time':
        return filtered.sort((a, b) => b.detectedAt - a.detectedAt)
      case 'severity':
        const severityOrder = { CRITICAL: 4, HIGH: 3, MEDIUM: 2, LOW: 1 }
        return filtered.sort((a, b) => severityOrder[b.threatLevel] - severityOrder[a.threatLevel])
      case 'name':
        return filtered.sort((a, b) => a.filePath.localeCompare(b.filePath))
      default:
        return filtered
    }
  }, [threats, searchQuery, sortBy, viewMode])

  // Count active threats
  const activeThreats = useMemo(() => {
    return threats.filter(t => !t.markedSafe).length
  }, [threats])

  // Update selected threat when threats change
  useMemo(() => {
    if (selectedThreat && !threats.find(t => t.id === selectedThreat.id)) {
      setSelectedThreat(filteredThreats.length > 0 ? filteredThreats[0] : null)
    }
  }, [threats, selectedThreat, filteredThreats])

  if (!isOpen) return null

  return (
    <div className="fixed inset-0 bg-black/60 backdrop-blur-sm z-50 flex items-center justify-center p-4">
      <div className="w-full max-w-7xl h-[90vh] bg-card rounded-2xl shadow-2xl flex flex-col overflow-hidden border border-border">
        <QuarantineHeader 
          threatCount={threats.length}
          onClose={onClose}
          multiSelectMode={multiSelectMode}
          onToggleMultiSelect={() => {
            setMultiSelectMode(!multiSelectMode)
            setSelectedThreats(new Set())
          }}
          selectedCount={selectedThreats.size}
        />

        <div className="px-6 py-4 border-b border-border">
          <QuarantineSearchBar 
            searchQuery={searchQuery}
            onSearchChange={setSearchQuery}
            sortBy={sortBy}
            onSortChange={setSortBy}
            viewMode={viewMode}
            onViewModeChange={setViewMode}
            totalThreats={threats.length}
            activeThreats={activeThreats}
          />
        </div>

        <div className="flex-1 flex overflow-hidden">
          <div className="w-2/5 border-r border-border overflow-hidden flex flex-col p-4">
            <ThreatList 
              threats={filteredThreats}
              selectedThreat={selectedThreat}
              onSelectThreat={setSelectedThreat}
              selectedThreats={selectedThreats}
              onToggleSelect={handleToggleSelect}
              onToggleSelectAll={handleToggleSelectAll}
              multiSelectMode={multiSelectMode}
            />
          </div>

          <div className="flex-1 flex flex-col overflow-hidden">
            <ThreatDetails threat={selectedThreat} />
            <ThreatActions 
              threat={selectedThreat}
              onDeleteThreat={onDeleteThreat}
              onMarkSafe={onMarkSafe}
              selectedThreats={selectedThreats}
              onBulkDelete={handleBulkDelete}
              onBulkMarkSafe={handleBulkMarkSafe}
              multiSelectMode={multiSelectMode}
            />
          </div>
        </div>
      </div>
    </div>
  )
}
