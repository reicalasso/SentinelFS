import { ChevronRight, ChevronDown, Folder, File, Trash2, CheckCircle2, Clock } from 'lucide-react'

interface FileTreeItemProps {
  item: any
  level: number
  expandedFolders: Set<string>
  toggleFolder: (path: string) => void
  formatSize: (bytes: number) => string
  handleRemoveWatch: (path: string, e: React.MouseEvent) => void
  isFiltering: boolean
}

export function FileTreeItem({ 
  item, 
  level, 
  expandedFolders, 
  toggleFolder, 
  formatSize, 
  handleRemoveWatch,
  isFiltering 
}: FileTreeItemProps) {
  const itemName = item.name || item.path.split('/').pop() || item.path
  const isExpanded = expandedFolders.has(item.path)
  const hasChildren = item.children && item.children.length > 0
  const canExpand = !isFiltering && item.isFolder
  
  const paddingLeft = `${level * 1 + 0.75}rem`
  
  return (
    <>
      {/* Mobile View */}
      <div 
        className={`group file-tree-item-mobile ${canExpand ? 'cursor-pointer' : ''}`}
        onClick={() => canExpand && toggleFolder(item.path)}
        style={{ paddingLeft: level === 0 ? '0.75rem' : paddingLeft }}
      >
        <div className="file-tree-item-content">
          {canExpand ? (
            <button className="file-tree-expand-btn">
              {isExpanded ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
            </button>
          ) : (
            <div className="w-6"></div> 
          )}
          
          <div className={`file-tree-icon ${item.isFolder ? 'file-tree-icon-folder' : 'file-tree-icon-file'}`}>
            {item.isFolder ? <Folder className="w-4 h-4" /> : <File className="w-4 h-4" />}
          </div>
          
          <div className="flex-1 min-w-0">
            <div className="file-tree-name">{itemName}</div>
            <div className="file-tree-meta">
              <span>{item.size !== undefined && item.size !== null ? formatSize(item.size) : '—'}</span>
              <span>•</span>
              <span className={`${
                item.syncStatus === 'synced' || item.syncStatus === 'watching'
                ? 'status-success' 
                : 'status-warning'
              }`}>{item.syncStatus}</span>
            </div>
          </div>
          
          {!isFiltering && !item.parent && (
            <button 
              onClick={(e) => handleRemoveWatch(item.path, e)}
              className="file-tree-remove-btn"
              title="Stop watching"
            >
              <Trash2 className="w-4 h-4" />
            </button>
          )}
        </div>
      </div>
      
      {/* Desktop View */}
      <div 
        className={`group file-tree-item-desktop ${canExpand ? 'cursor-pointer select-none' : ''}`}
        onClick={() => canExpand && toggleFolder(item.path)}
        style={{ paddingLeft: level === 0 ? '1.25rem' : `${level * 1.5 + 1}rem` }}
      >
        <div className="col-span-6 flex items-center gap-3 overflow-hidden">
          {canExpand ? (
            <button className="file-tree-expand-btn">
              {isExpanded ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
            </button>
          ) : (
            <div className="w-6"></div> 
          )}
          
          <div className={`file-tree-icon file-tree-icon-hover ${item.isFolder ? 'file-tree-icon-folder' : 'file-tree-icon-file'}`}>
            {item.isFolder ? <Folder className="w-4 h-4" /> : <File className="w-4 h-4" />}
          </div>
          
          <div className="truncate min-w-0 flex-1">
            <div className="file-tree-name-desktop" title={item.path}>{itemName}</div>
            {(level === 0 || isFiltering) && (
              <div className="file-tree-path">{item.path}</div>
            )}
          </div>
        </div>
        
        <div className="col-span-2 font-mono text-muted-foreground text-xs">
          {item.size !== undefined && item.size !== null ? formatSize(item.size) : '—'}
        </div>
        
        <div className="col-span-2">
          <span className={`file-tree-status ${
            item.syncStatus === 'synced' || item.syncStatus === 'watching'
            ? 'file-tree-status-synced' 
            : 'file-tree-status-pending'
          }`}>
            {item.syncStatus === 'synced' || item.syncStatus === 'watching' ? (
              <CheckCircle2 className="w-3 h-3" />
            ) : (
              <Clock className="w-3 h-3" />
            )}
            {item.syncStatus}
          </span>
        </div>
        
        <div className="col-span-2 text-right">
          {!isFiltering && !item.parent && (
            <button 
              onClick={(e) => handleRemoveWatch(item.path, e)}
              className="file-tree-remove-btn-desktop"
              title="Stop watching"
            >
              <Trash2 className="w-4 h-4" />
            </button>
          )}
        </div>
      </div>

      {/* Recursive Children */}
      {canExpand && isExpanded && hasChildren && (
        <div className="file-tree-children">
          {item.children.map((child: any) => (
            <FileTreeItem 
              key={child.path} 
              item={child} 
              level={level + 1} 
              expandedFolders={expandedFolders} 
              toggleFolder={toggleFolder} 
              formatSize={formatSize}
              handleRemoveWatch={handleRemoveWatch}
              isFiltering={isFiltering}
            />
          ))}
        </div>
      )}
    </>
  )
}
