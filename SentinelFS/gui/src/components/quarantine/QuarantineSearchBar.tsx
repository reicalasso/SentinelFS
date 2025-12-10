import { Search, X } from 'lucide-react'
import { SortBy, ViewMode } from './types'

interface QuarantineSearchBarProps {
  searchQuery: string
  onSearchChange: (query: string) => void
  sortBy: SortBy
  onSortChange: (sort: SortBy) => void
  viewMode: ViewMode
  onViewModeChange: (mode: ViewMode) => void
  totalThreats: number
  activeThreats: number
}

export function QuarantineSearchBar({
  searchQuery,
  onSearchChange,
  sortBy,
  onSortChange,
  viewMode,
  onViewModeChange,
  totalThreats,
  activeThreats
}: QuarantineSearchBarProps) {
  return (
    <div className="space-y-4">
      {/* Search Bar */}
      <div className="relative">
        <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-4 h-4 text-muted-foreground" />
        <input
          type="text"
          placeholder="Search threats..."
          value={searchQuery}
          onChange={(e) => onSearchChange(e.target.value)}
          className="w-full pl-10 pr-10 py-2.5 bg-background/50 border border-border rounded-lg focus:outline-none focus:ring-2 focus:ring-primary/50 transition-all"
        />
        {searchQuery && (
          <button
            onClick={() => onSearchChange('')}
            className="absolute right-3 top-1/2 -translate-y-1/2 text-muted-foreground hover:text-foreground transition-colors"
          >
            <X className="w-4 h-4" />
          </button>
        )}
      </div>

      {/* Filters */}
      <div className="flex flex-wrap gap-2 items-center justify-between">
        {/* View Mode Tabs */}
        <div className="flex gap-1 p-1 bg-background/50 rounded-lg border border-border">
          <button
            onClick={() => onViewModeChange('all')}
            className={`px-4 py-1.5 rounded text-sm font-medium transition-all ${
              viewMode === 'all'
                ? 'bg-primary text-primary-foreground shadow-sm'
                : 'text-muted-foreground hover:text-foreground'
            }`}
          >
            Tümü ({totalThreats})
          </button>
          <button
            onClick={() => onViewModeChange('active')}
            className={`px-4 py-1.5 rounded text-sm font-medium transition-all ${
              viewMode === 'active'
                ? 'bg-primary text-primary-foreground shadow-sm'
                : 'text-muted-foreground hover:text-foreground'
            }`}
          >
            Aktif ({activeThreats})
          </button>
          <button
            onClick={() => onViewModeChange('safe')}
            className={`px-4 py-1.5 rounded text-sm font-medium transition-all ${
              viewMode === 'safe'
                ? 'bg-primary text-primary-foreground shadow-sm'
                : 'text-muted-foreground hover:text-foreground'
            }`}
          >
            Güvenli İşaretli ({totalThreats - activeThreats})
          </button>
        </div>

        {/* Sort Options */}
        <div className="flex items-center gap-2">
          <span className="text-sm text-muted-foreground">Sort by:</span>
          <select
            value={sortBy}
            onChange={(e) => onSortChange(e.target.value as SortBy)}
            className="px-3 py-1.5 bg-background/50 border border-border rounded-lg text-sm focus:outline-none focus:ring-2 focus:ring-primary/50 transition-all"
          >
            <option value="time">Detection Time</option>
            <option value="severity">Threat Level</option>
            <option value="name">File Name</option>
          </select>
        </div>
      </div>
    </div>
  )
}
