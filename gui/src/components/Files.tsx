import { useState, useMemo } from 'react'
import { FolderOpen, Plus, File, Search, Trash2, ChevronRight, ChevronDown, Folder, HardDrive, CheckCircle2, Clock, RefreshCw } from 'lucide-react'

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
  const { tree: fileTree, flatList: flatItems } = useMemo(() => {
      if (!files) return { tree: [], flatList: [] }

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

      const roots = Array.from(map.values()).filter(f => f.isFolder && !f.parent && files.some(rf => rf.path === f.path && rf.isFolder && !rf.parent))

      const computeFolderSize = (node: any): number => {
          if (!node.children || node.children.length === 0) {
              node.size = node.size ?? 0
              return node.size
          }

          let total = node.isFolder ? 0 : (node.size ?? 0)
          node.children.forEach((child: any) => {
              total += computeFolderSize(child)
          })

          node.size = total
          return node.size
      }

      roots.forEach(root => computeFolderSize(root))

      return {
          tree: roots,
          flatList: Array.from(map.values())
      }
  }, [files])

  // Filter handling
  const isFiltering = searchQuery.length > 0 || filterType !== 'All Types'
  
  // If filtering, we flatten the structure again to show matches
  const displayItems = isFiltering ? (flatItems || []).filter((f: any) => {
      const name = f.path.split('/').pop() || f.path
      const matchesSearch = name.toLowerCase().includes(searchQuery.toLowerCase())
      const matchesType = filterType === 'All Types' 
          ? true 
          : filterType === 'Folders' ? f.isFolder 
          : !f.isFolder
      return matchesSearch && matchesType
  }) : fileTree

  const hasRealData = fileTree && fileTree.length > 0

  // Calculate stats
  const stats = useMemo(() => {
    if (!flatItems) return { totalFiles: 0, totalFolders: 0, totalSize: 0 }
    const files = flatItems.filter((f: any) => !f.isFolder)
    const folders = flatItems.filter((f: any) => f.isFolder)
    const totalSize = files.reduce((acc: number, f: any) => acc + (f.size || 0), 0)
    return { totalFiles: files.length, totalFolders: folders.length, totalSize }
  }, [flatItems])

  return (
    <div className="space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
      {/* Hero Section */}
      <div className="relative overflow-hidden rounded-3xl bg-gradient-to-br from-emerald-500/20 via-card to-primary/10 border border-emerald-500/20 p-8">
        {/* Animated Background */}
        <div className="absolute inset-0 overflow-hidden">
          <div className="absolute -top-1/2 -left-1/4 w-96 h-96 bg-emerald-500/20 rounded-full blur-3xl animate-pulse"></div>
          <div className="absolute -bottom-1/4 -right-1/4 w-96 h-96 bg-primary/20 rounded-full blur-3xl animate-pulse" style={{animationDelay: '1s'}}></div>
          {/* File Pattern */}
          <div className="absolute inset-0 bg-[radial-gradient(circle_at_center,rgba(255,255,255,0.02)_1px,transparent_1px)] bg-[size:20px_20px]"></div>
        </div>
        
        <div className="relative z-10">
          <div className="flex items-center justify-between mb-6">
            <div className="flex items-center gap-4">
              <div className="p-4 rounded-2xl bg-gradient-to-br from-emerald-500/20 to-primary/20 backdrop-blur-sm border border-emerald-500/30 shadow-lg shadow-emerald-500/20">
                <FolderOpen className="w-8 h-8 text-emerald-400" />
              </div>
              <div>
                <h2 className="text-2xl font-bold bg-gradient-to-r from-foreground to-foreground/60 bg-clip-text text-transparent">
                  SYNC Files
                </h2>
                <p className="text-sm text-muted-foreground mt-1 flex items-center gap-2">
                  <span className="w-2 h-2 rounded-full bg-emerald-400 animate-pulse"></span>
                  Manage synchronized directories
                </p>
              </div>
            </div>
            
            <button 
              onClick={handleAddFolder}
              className="relative overflow-hidden flex items-center gap-2.5 bg-gradient-to-r from-emerald-500 to-emerald-600 hover:from-emerald-400 hover:to-emerald-500 text-white px-5 py-2.5 rounded-xl text-sm font-semibold transition-all shadow-lg shadow-emerald-500/30 hover:shadow-emerald-500/50 hover:scale-105 active:scale-95"
            >
              {/* Shine effect */}
              <div className="absolute inset-0 bg-gradient-to-r from-transparent via-white/20 to-transparent translate-x-[-100%] hover:translate-x-[100%] transition-transform duration-500"></div>
              <Plus className="w-4 h-4 relative" />
              <span className="relative">Add Folder</span>
            </button>
          </div>
          
          {/* Stats Row */}
          <div className="grid grid-cols-3 gap-4">
            <div className="flex items-center gap-3 p-3 rounded-xl bg-gradient-to-r from-emerald-500/15 to-emerald-600/10 border border-emerald-500/30 backdrop-blur-sm">
              <div className="p-2 rounded-lg bg-background/50">
                <Folder className="w-4 h-4 text-emerald-400" />
              </div>
              <div>
                <div className="text-lg font-bold text-emerald-400">{stats.totalFolders}</div>
                <div className="text-[10px] text-muted-foreground uppercase tracking-wider">Folders</div>
              </div>
            </div>
            <div className="flex items-center gap-3 p-3 rounded-xl bg-gradient-to-r from-violet-500/15 to-violet-600/10 border border-violet-500/30 backdrop-blur-sm">
              <div className="p-2 rounded-lg bg-background/50">
                <File className="w-4 h-4 text-violet-400" />
              </div>
              <div>
                <div className="text-lg font-bold text-violet-400">{stats.totalFiles}</div>
                <div className="text-[10px] text-muted-foreground uppercase tracking-wider">Files</div>
              </div>
            </div>
            <div className="flex items-center gap-3 p-3 rounded-xl bg-gradient-to-r from-primary/15 to-primary/10 border border-primary/30 backdrop-blur-sm">
              <div className="p-2 rounded-lg bg-background/50">
                <HardDrive className="w-4 h-4 text-primary" />
              </div>
              <div>
                <div className="text-lg font-bold text-primary">{formatSize(stats.totalSize)}</div>
                <div className="text-[10px] text-muted-foreground uppercase tracking-wider">Total Size</div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Filters & Search - Enhanced */}
      <div className="flex gap-4">
        <div className="relative flex-1 max-w-md group">
            <Search className="absolute left-4 top-3 w-4 h-4 text-muted-foreground group-focus-within:text-primary transition-colors" />
            <input 
                type="text" 
                placeholder="Search files and folders..." 
                value={searchQuery}
                onChange={(e) => setSearchQuery(e.target.value)}
                className="w-full bg-gradient-to-r from-secondary/60 to-secondary/40 border border-border/50 rounded-xl pl-11 pr-4 py-3 text-sm focus:ring-2 focus:ring-primary/30 focus:border-primary/50 outline-none transition-all placeholder:text-muted-foreground/60"
            />
        </div>
        <select 
            value={filterType}
            onChange={(e) => setFilterType(e.target.value)}
            className="bg-gradient-to-r from-secondary/60 to-secondary/40 border border-border/50 rounded-xl px-5 py-3 text-sm outline-none focus:ring-2 focus:ring-primary/30 focus:border-primary/50 transition-all cursor-pointer"
        >
            <option>All Types</option>
            <option>Folders</option>
            <option>Files</option>
        </select>
      </div>

      {/* File List - Enhanced */}
      <div className="card-modern overflow-hidden">
        {/* Header */}
        <div className="grid grid-cols-12 gap-4 p-5 border-b border-border/30 text-xs font-bold text-muted-foreground/80 uppercase tracking-widest bg-gradient-to-r from-secondary/50 to-secondary/30">
            <div className="col-span-6 flex items-center gap-2">
              <File className="w-3.5 h-3.5" />
              Name
            </div>
            <div className="col-span-2 flex items-center gap-2">
              <HardDrive className="w-3.5 h-3.5" />
              Size
            </div>
            <div className="col-span-2 flex items-center gap-2">
              <RefreshCw className="w-3.5 h-3.5" />
              Status
            </div>
            <div className="col-span-2 text-right">Actions</div>
        </div>
        
        <div className="divide-y divide-border/20">
            {!hasRealData && (
                <div className="p-16 text-center flex flex-col items-center">
                    <div className="relative mb-6">
                        <div className="absolute inset-0 bg-emerald-500/20 blur-3xl rounded-full"></div>
                        <div className="relative p-6 rounded-3xl bg-gradient-to-br from-secondary to-secondary/50 border border-border/50">
                            <FolderOpen className="w-12 h-12 text-muted-foreground/30" />
                        </div>
                    </div>
                    <h3 className="font-bold text-lg text-foreground mb-2">No Synced Files Yet</h3>
                    <p className="text-muted-foreground max-w-sm">Add a folder to start synchronizing your files across devices.</p>
                    <button 
                        onClick={handleAddFolder}
                        className="mt-6 px-6 py-3 bg-emerald-500/10 hover:bg-emerald-500/20 text-emerald-400 rounded-xl font-medium transition-all border border-emerald-500/20"
                    >
                        Add Your First Folder
                    </button>
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
                className={`grid grid-cols-12 gap-4 p-4 hover:bg-gradient-to-r hover:from-secondary/40 hover:to-transparent transition-all items-center group text-sm ${canExpand ? 'cursor-pointer select-none' : ''}`}
                onClick={() => canExpand && toggleFolder(item.path)}
                style={{ paddingLeft: level === 0 ? '1.25rem' : paddingLeft }}
            >
                <div className="col-span-6 flex items-center gap-3 overflow-hidden">
                    {canExpand ? (
                        <button className="p-1 hover:bg-secondary rounded-lg text-muted-foreground hover:text-primary transition-all">
                            {isExpanded ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
                        </button>
                    ) : (
                        <div className="w-6"></div> 
                    )}
                    
                    <div className={`p-2 rounded-xl transition-all group-hover:scale-110 ${
                        item.isFolder 
                            ? 'bg-gradient-to-br from-amber-500/20 to-amber-600/10 text-amber-400 border border-amber-500/30' 
                            : 'bg-gradient-to-br from-violet-500/20 to-violet-600/10 text-violet-400 border border-violet-500/30'
                    }`}>
                        {item.isFolder ? <Folder className="w-4 h-4" /> : <File className="w-4 h-4" />}
                    </div>
                    
                    <div className="truncate min-w-0 flex-1">
                        <div className="font-semibold truncate group-hover:text-primary transition-colors" title={item.path}>{itemName}</div>
                        {/* Show path for root items or when filtering */}
                        {(level === 0 || isFiltering) && (
                            <div className="text-[10px] text-muted-foreground/60 truncate font-mono">{item.path}</div>
                        )}
                    </div>
                </div>
                
                <div className="col-span-2 font-mono text-muted-foreground text-xs">
                    {item.size !== undefined && item.size !== null ? formatSize(item.size) : '—'}
                </div>
                
                <div className="col-span-2">
                     <span className={`inline-flex items-center gap-1.5 text-[10px] px-2.5 py-1 rounded-lg uppercase font-bold tracking-wide ${
                        item.syncStatus === 'synced' || item.syncStatus === 'watching'
                        ? 'bg-emerald-500/15 text-emerald-400 border border-emerald-500/30' 
                        : 'bg-amber-500/15 text-amber-400 border border-amber-500/30 animate-pulse'
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
                            className="p-2 hover:bg-destructive/15 hover:text-destructive rounded-xl text-muted-foreground/50 transition-all opacity-0 group-hover:opacity-100 border border-transparent hover:border-destructive/30"
                            title="Stop watching"
                        >
                            <Trash2 className="w-4 h-4" />
                        </button>
                    )}
                </div>
            </div>

            {/* Recursive Children */}
            {canExpand && isExpanded && hasChildren && (
                <div className="border-l-2 border-primary/20 ml-6 bg-gradient-to-r from-primary/5 to-transparent">
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
