import { Search } from 'lucide-react'

interface FilesSearchProps {
  searchQuery: string
  onSearchChange: (query: string) => void
  filterType: string
  onFilterChange: (filter: string) => void
}

export function FilesSearch({ searchQuery, onSearchChange, filterType, onFilterChange }: FilesSearchProps) {
  return (
    <div className="files-search-container">
      <div className="files-search-input-wrapper">
        <Search className="files-search-icon" />
        <input 
          type="text" 
          placeholder="Search files and folders..." 
          value={searchQuery}
          onChange={(e) => onSearchChange(e.target.value)}
          className="files-search-input"
        />
      </div>
      <select 
        value={filterType}
        onChange={(e) => onFilterChange(e.target.value)}
        className="files-filter-select"
      >
        <option>All Types</option>
        <option>Folders</option>
        <option>Files</option>
      </select>
    </div>
  )
}
