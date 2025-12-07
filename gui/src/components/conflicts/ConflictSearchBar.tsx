import { Search, Filter } from 'lucide-react'
import { SortBy } from './types'

interface ConflictSearchBarProps {
  searchQuery: string
  onSearchChange: (query: string) => void
  sortBy: SortBy
  onSortChange: (sort: SortBy) => void
}

export function ConflictSearchBar({ searchQuery, onSearchChange, sortBy, onSortChange }: ConflictSearchBarProps) {
  return (
    <div className="conflict-search-bar">
      <div className="conflict-search-input-wrapper">
        <Search className="conflict-search-icon" />
        <input
          type="text"
          value={searchQuery}
          onChange={(e) => onSearchChange(e.target.value)}
          placeholder="Search files..."
          className="conflict-search-input"
        />
      </div>
      <div className="conflict-filter-wrapper">
        <Filter className="w-4 h-4 text-muted-foreground" />
        <select
          value={sortBy}
          onChange={(e) => onSortChange(e.target.value as SortBy)}
          className="conflict-filter-select"
        >
          <option value="time">Sort by Time</option>
          <option value="name">Sort by Name</option>
          <option value="size">Sort by Size</option>
        </select>
      </div>
    </div>
  )
}
