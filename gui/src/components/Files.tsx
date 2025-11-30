import { useState, useMemo } from 'react'
import { FolderOpen, Plus, File, Search, Trash2, ChevronRight, ChevronDown, Folder } from 'lucide-react'

export function Files({ files }: { files?: any[] }) {
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(new Set())
  const [searchQuery, setSearchQuery] = useState('')
  const [filterType, setFilterType] = useState('All Types')
  
  const formatSize = (bytes: number) => {
    if (bytes < 1024) return bytes + ' B'
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  }
  
  const handleAddFolder = async () => {
    if (window.api) {
      const result = await window.api.selectFolder()
      if (result) {
        const response = await window.api.sendCommand(`ADD_FOLDER ${result}`)
        if (response.success) {
          alert(`✓ Folder added successfully:\n${result}`)
        } else {
          alert(`✗ Failed to add folder:\n${response.error || 'Unknown error'}`)
        }
      }
    }
  }
  
  const toggleFolder = (path: string) => {
    const newExpanded = new Set(expandedFolders)
    if (newExpanded.has(path)) {
      newExpanded.delete(path)
    } else {
      newExpanded.add(path)
    }
    setExpandedFolders(newExpanded)
  }
  
  const handleRemoveWatch = async (path: string, e: React.MouseEvent) => {
    e.stopPropagation()
    if (!window.api) return
    
    if (confirm(`Stop watching:\n${path}\n\nThis will not delete the files, only stop syncing them.`)) {
      const response = await window.api.sendCommand(`REMOVE_WATCH ${path}`)
      if (response.success) {
        alert(`✓ Stopped watching:\n${path}`)
      } else {
        alert(`✗ Failed to stop watching:\n${response.error || 'Unknown error'}`)
      }
    }
  }

  // Reconstruct the file tree from flat list
  const fileTree = useMemo(() => {
      if (!files) return []

      const map = new Map<string, any>()
      
      // 1. Initialize map with provided files/folders
      files.forEach(f => {
          map.set(f.path, { ...f, children: [] })
      })

      const getOrCreateFolder = (path: string, rootPath: string): any => {
          if (map.has(path)) return map.get(path)
          
          // Virtual folder creation
          const newFolder = {
              path,
              name: path.split('/').pop(),
              isFolder: true,
              size: 0,
              syncStatus: 'synced',
              children: [],
              // parent will be set when attached
          }
          map.set(path, newFolder)

          // Attach to parent if possible
          if (path !== rootPath && path.length > rootPath.length) {
               // Find immediate parent path
               // Handle both / and \ just in case, though Linux uses /
               const lastSlash = path.lastIndexOf('/')
               if (lastSlash > 0) {
                   const parentPath = path.substring(0, lastSlash)
                   // Recursively ensure parent exists up to rootPath
                   if (parentPath.length >= rootPath.length) {
                        const parentNode = getOrCreateFolder(parentPath, rootPath)
                        if (!parentNode.children.find((c: any) => c.path === path)) {
                            parentNode.children.push(newFolder)
                        }
                   }
               }
          }
          return newFolder
      }

      // 2. Process files to build hierarchy
      files.forEach(f => {
          // Skip explicit roots for now, they are handled as entry points
          if (f.isFolder && !f.parent) return

          const rootPath = f.parent
          if (!rootPath) return // Should not happen for non-roots

          // Find the directory containing this file
          const lastSlash = f.path.lastIndexOf('/')
          const parentDir = f.path.substring(0, lastSlash)

          if (parentDir === rootPath) {
              // Direct child of root
              const rootNode = map.get(rootPath)
              if (rootNode) {
                  if (!rootNode.children.find((c: any) => c.path === f.path)) {
                      rootNode.children.push(map.get(f.path))
                  }
              }
          } else {
              // Nested deep
              const parentFolder = getOrCreateFolder(parentDir, rootPath)
              if (!parentFolder.children.find((c: any) => c.path === f.path)) {
                  parentFolder.children.push(map.get(f.path))
              }
          }
      })

      // Return only the top-level roots (watched folders)
      return Array.from(map.values()).filter(f => f.isFolder && !f.parent && files.some(rf => rf.path === f.path && rf.isFolder && !rf.parent))
  }, [files])

  // Filter handling
  const isFiltering = searchQuery.length > 0 || filterType !== 'All Types'
  
  // If filtering, we flatten the structure again to show matches
  const displayItems = isFiltering ? (files || []).filter((f: any) => {
      const name = f.path.split('/').pop() || f.path
      const matchesSearch = name.toLowerCase().includes(searchQuery.toLowerCase())
      const matchesType = filterType === 'All Types' 
          ? true 
          : filterType === 'Folders' ? f.isFolder 
          : !f.isFolder
      return matchesSearch && matchesType
  }) : fileTree

  const hasRealData = files && files.length > 0

  return (
    <div className="space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
      <div className="flex items-center justify-between">
        <div>
            <h2 className="text-lg font-semibold flex items-center gap-2">
                <FolderOpen className="w-5 h-5 text-primary" />
                Synced Folders
            </h2>
            <p className="text-sm text-muted-foreground">Manage your synchronized directories and files</p>
        </div>
        <button 
          onClick={handleAddFolder}
          className="flex items-center gap-2 bg-primary hover:bg-primary/90 text-primary-foreground px-4 py-2 rounded-lg text-sm font-medium transition-colors shadow-sm shadow-primary/20"
        >
          <Plus className="w-4 h-4" />
          Add Folder
        </button>
      </div>

      {/* Filters & Search */}
      <div className="flex gap-4">
        <div className="relative flex-1 max-w-md">
            <Search className="absolute left-3 top-2.5 w-4 h-4 text-muted-foreground" />
            <input 
                type="text" 
                placeholder="Search files..." 
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="w-full bg-secondary/50 border border-border rounded-lg pl-10 pr-4 py-2 text-sm focus:ring-1 focus:ring-primary/50 outline-none transition-all"
            />
        </div>
        <select 
            value={filterType}
            onChange={(e) => setFilterType(e.target.value)}
            className="bg-secondary/50 border border-border rounded-lg px-4 py-2 text-sm outline-none"
        >
            <option>All Types</option>
            <option>Folders</option>
            <option>Files</option>
        </select>
      </div>

      {/* File List */}
      <div className="bg-card/50 backdrop-blur-sm border border-border/50 rounded-xl overflow-hidden shadow-sm">
        <div className="grid grid-cols-12 gap-4 p-4 border-b border-border/50 text-xs font-medium text-muted-foreground uppercase tracking-wider bg-secondary/30">
            <div className="col-span-6">Name</div>
            <div className="col-span-2">Size</div>
            <div className="col-span-2">Status</div>
            <div className="col-span-2 text-right">Actions</div>
        </div>
        <div className="divide-y divide-border/30">
            {!hasRealData && (
                <div className="p-12 text-center text-muted-foreground flex flex-col items-center">
                    <div className="bg-secondary/50 p-4 rounded-full mb-4">
                        <FolderOpen className="w-8 h-8 opacity-50" />
                    </div>
                    <p>No synced files yet. Add a folder to start syncing.</p>
                </div>
            )}
            {displayItems.map((item: any, i: number) => (
                <FileTreeItem 
                    key={item.path} 
                    item={item} 
                    level={0} 
                    expandedFolders={expandedFolders} 
                    toggleFolder={toggleFolder} 
                    formatSize={formatSize} 
                    handleRemoveWatch={handleRemoveWatch}
                    isFiltering={isFiltering}
                />
            ))}
        </div>
      </div>
    </div>
  )
}

function FileTreeItem({ item, level, expandedFolders, toggleFolder, formatSize, handleRemoveWatch, isFiltering }: any) {
    const itemName = item.name || item.path.split('/').pop() || item.path
    const isExpanded = expandedFolders.has(item.path)
    const hasChildren = item.children && item.children.length > 0
    const canExpand = !isFiltering && item.isFolder
    
    // Indentation style
    const paddingLeft = `${level * 1.5 + 1}rem`
    
    return (
        <>
            <div 
                className={`grid grid-cols-12 gap-4 p-3 hover:bg-secondary/40 transition-colors items-center group text-sm ${canExpand ? 'cursor-pointer select-none' : ''}`}
                onClick={() => canExpand && toggleFolder(item.path)}
                style={{ paddingLeft: level === 0 ? '1rem' : paddingLeft }}
            >
                <div className="col-span-6 flex items-center gap-3 overflow-hidden">
                    {canExpand ? (
                        <button className="p-0.5 hover:bg-secondary rounded text-muted-foreground">
                            {isExpanded ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
                        </button>
                    ) : (
                        <div className="w-5"></div> 
                    )}
                    
                    <div className={`p-1.5 rounded-lg ${item.isFolder ? 'bg-primary/10 text-primary' : 'bg-violet-500/10 text-violet-500'}`}>
                        {item.isFolder ? <Folder className="w-4 h-4" /> : <File className="w-4 h-4" />}
                    </div>
                    
                    <div className="truncate min-w-0 flex-1">
                        <div className="font-medium truncate" title={item.path}>{itemName}</div>
                        {/* Show path for root items or when filtering */}
                        {(level === 0 || isFiltering) && <div className="text-[10px] text-muted-foreground truncate opacity-60">{item.path}</div>}
                    </div>
                </div>
                
                <div className="col-span-2 font-mono text-muted-foreground text-xs">
                    {item.isFolder ? '-' : formatSize(item.size)}
                </div>
                
                <div className="col-span-2">
                     <span className={`text-[10px] px-2 py-0.5 rounded-full uppercase font-bold tracking-wide ${
                        item.syncStatus === 'synced' || item.syncStatus === 'watching'
                        ? 'bg-emerald-500/10 text-emerald-500 border border-emerald-500/20' 
                        : 'bg-amber-500/10 text-amber-500 border border-amber-500/20 animate-pulse'
                    }`}>
                        {item.syncStatus}
                    </span>
                </div>
                
                <div className="col-span-2 text-right">
                    {!isFiltering && !item.parent && (
                        <button 
                            onClick={(e) => handleRemoveWatch(item.path, e)}
                            className="p-1.5 hover:bg-destructive/10 hover:text-destructive rounded-md text-muted-foreground transition-all opacity-0 group-hover:opacity-100"
                            title="Stop watching"
                        >
                            <Trash2 className="w-4 h-4" />
                        </button>
                    )}
                </div>
            </div>

            {/* Recursive Children */}
            {canExpand && isExpanded && hasChildren && (
                <div className="border-l border-border/30 ml-4 md:ml-6">
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
