import { FolderOpen, Plus, MoreVertical, CheckCircle, AlertCircle, Clock, File, Search, Trash2, ChevronRight, ChevronDown, Folder } from 'lucide-react'
import { useState } from 'react'

const mockFolders = [
  { name: '~/Documents', files: 1247, size: '4.2 GB', status: 'synced', lastSync: '2 min ago' },
  { name: '~/Pictures', files: 3891, size: '12.8 GB', status: 'syncing', lastSync: 'Now' },
  { name: '~/Projects', files: 892, size: '2.1 GB', status: 'synced', lastSync: '5 min ago' },
  { name: '~/Music', files: 456, size: '8.4 GB', status: 'pending', lastSync: 'Never' },
]

export function Files({ files }: { files?: any[] }) {
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(new Set())
  
  const formatSize = (bytes: number) => {
    if (bytes < 1024) return bytes + ' B'
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  }
  
  const formatDate = (timestamp: number) => {
    const date = new Date(timestamp * 1000)
    const now = new Date()
    const diff = Math.floor((now.getTime() - date.getTime()) / 1000)
    
    if (diff < 60) return 'Just now'
    if (diff < 3600) return Math.floor(diff / 60) + ' min ago'
    if (diff < 86400) return Math.floor(diff / 3600) + ' hours ago'
    return date.toLocaleDateString()
  }
  
  const handleAddFolder = async () => {
    if (window.api) {
      const result = await window.api.selectFolder()
      if (result) {
        console.log('Selected folder:', result)
        
        // Send command to daemon to add this folder to watch list
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
    e.stopPropagation() // Prevent folder toggle
    
    if (!window.api) return
    
    if (confirm(`Stop watching:\n${path}\n\nThis will not delete the files, only stop syncing them.`)) {
      const response = await window.api.sendCommand(`REMOVE_WATCH ${path}`)
      
      if (response.success) {
        alert(`✓ Stopped watching:\n${path}`)
        // Refresh will happen automatically via daemon data update
      } else {
        alert(`✗ Failed to stop watching:\n${response.error || 'Unknown error'}`)
      }
    }
  }
  
  // Organize files by parent folder
  const displayFiles = (files && files.length > 0) ? files : []
  const rootItems = displayFiles.filter((f: any) => !f.parent)
  const childrenMap = new Map<string, any[]>()
  
  displayFiles.forEach((f: any) => {
    if (f.parent) {
      if (!childrenMap.has(f.parent)) {
        childrenMap.set(f.parent, [])
      }
      childrenMap.get(f.parent)!.push(f)
    }
  })
  
  const hasRealData = files && files.length > 0
  return (
    <div className="space-y-6 animate-in fade-in duration-500">
      <div className="flex items-center justify-between">
        <div>
            <h2 className="text-lg font-semibold">Synced Folders</h2>
            <p className="text-sm text-muted-foreground">Manage your synchronized directories and files</p>
        </div>
        <button 
          onClick={handleAddFolder}
          className="flex items-center gap-2 bg-blue-600 hover:bg-blue-700 text-white px-4 py-2 rounded-lg text-sm font-medium transition-colors"
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
                className="w-full bg-secondary/50 border border-border rounded-lg pl-10 pr-4 py-2 text-sm focus:ring-1 focus:ring-blue-500 outline-none"
            />
        </div>
        <select className="bg-secondary/50 border border-border rounded-lg px-4 py-2 text-sm outline-none">
            <option>All Types</option>
            <option>Folders</option>
            <option>Files</option>
        </select>
      </div>

      {/* File List */}
      <div className="bg-card border border-border rounded-xl overflow-hidden shadow-sm">
        <div className="grid grid-cols-12 gap-4 p-4 border-b border-border text-xs font-medium text-muted-foreground uppercase tracking-wider">
            <div className="col-span-5">Name</div>
            <div className="col-span-3">Path</div>
            <div className="col-span-2">Size</div>
            <div className="col-span-2 text-right">Status</div>
        </div>
        <div className="divide-y divide-border">
            {!hasRealData && (
                <div className="p-8 text-center text-muted-foreground">
                    <FolderOpen className="w-12 h-12 mx-auto mb-3 opacity-50" />
                    <p>No synced files yet. Add a folder to start syncing.</p>
                </div>
            )}
            {rootItems.map((item: any, i: number) => {
                const itemName = item.path.split('/').pop() || item.path
                const isExpanded = expandedFolders.has(item.path)
                const children = childrenMap.get(item.path) || []
                
                return (
                    <div key={i}>
                        {/* Root item (folder or file) */}
                        <div 
                            className="grid grid-cols-12 gap-4 p-4 hover:bg-secondary/30 transition-colors items-center group cursor-pointer"
                            onClick={() => item.isFolder && toggleFolder(item.path)}
                        >
                            <div className="col-span-5 flex items-center gap-3">
                                {item.isFolder && (
                                    <button className="p-1 hover:bg-secondary rounded">
                                        {isExpanded ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
                                    </button>
                                )}
                                <div className={`p-2 rounded-lg ${item.isFolder ? 'bg-blue-500/10 text-blue-500' : 'bg-purple-500/10 text-purple-500'}`}>
                                    {item.isFolder ? <Folder className="w-4 h-4" /> : <File className="w-4 h-4" />}
                                </div>
                                <div>
                                    <div className="font-medium text-sm">{itemName}</div>
                                    <div className="text-xs text-muted-foreground lg:hidden">{item.path}</div>
                                </div>
                            </div>
                            <div className="col-span-3 text-sm text-muted-foreground truncate hidden lg:block" title={item.path}>{item.path}</div>
                            <div className="col-span-2 text-sm text-muted-foreground font-mono">{formatSize(item.size)}</div>
                            <div className="col-span-2 flex items-center justify-end gap-4">
                                <span className={`text-xs px-2 py-1 rounded-full ${
                                    item.syncStatus === 'synced' || item.syncStatus === 'watching'
                                    ? 'bg-green-500/10 text-green-500' 
                                    : 'bg-blue-500/10 text-blue-500 animate-pulse'
                                }`}>
                                    {item.syncStatus}
                                </span>
                                <button 
                                    onClick={(e) => handleRemoveWatch(item.path, e)}
                                    className="p-1 hover:bg-secondary rounded text-muted-foreground hover:text-red-500 opacity-0 group-hover:opacity-100 transition-all"
                                    title="Stop watching"
                                >
                                    <Trash2 className="w-4 h-4" />
                                </button>
                            </div>
                        </div>
                        
                        {/* Children (shown when expanded) */}
                        {item.isFolder && isExpanded && children.map((child: any, j: number) => {
                            const childName = child.path.split('/').pop() || child.path
                            const isChildExpanded = expandedFolders.has(child.path)
                            const grandchildren = childrenMap.get(child.path) || []
                            
                            return (
                                <div key={`${i}-${j}`}>
                                    <div 
                                        className="grid grid-cols-12 gap-4 p-4 pl-16 hover:bg-secondary/30 transition-colors items-center group bg-secondary/10 cursor-pointer"
                                        onClick={() => child.isFolder && toggleFolder(child.path)}
                                    >
                                        <div className="col-span-5 flex items-center gap-3">
                                            {child.isFolder && (
                                                <button className="p-1 hover:bg-secondary rounded">
                                                    {isChildExpanded ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
                                                </button>
                                            )}
                                            <div className={`p-2 rounded-lg ${child.isFolder ? 'bg-blue-500/10 text-blue-500' : 'bg-purple-500/10 text-purple-500'}`}>
                                                {child.isFolder ? <Folder className="w-4 h-4" /> : <File className="w-4 h-4" />}
                                            </div>
                                            <div>
                                                <div className="font-medium text-sm">{childName}</div>
                                            </div>
                                        </div>
                                        <div className="col-span-3 text-sm text-muted-foreground truncate hidden lg:block" title={child.path}>{child.path}</div>
                                        <div className="col-span-2 text-sm text-muted-foreground font-mono">{formatSize(child.size)}</div>
                                        <div className="col-span-2 flex items-center justify-end gap-4">
                                            <span className={`text-xs px-2 py-1 rounded-full ${
                                                child.syncStatus === 'synced' || child.syncStatus === 'watching'
                                                ? 'bg-green-500/10 text-green-500' 
                                                : 'bg-blue-500/10 text-blue-500 animate-pulse'
                                            }`}>
                                                {child.syncStatus}
                                            </span>
                                        </div>
                                    </div>
                                    
                                    {/* Grandchildren (files in subdirectory) */}
                                    {child.isFolder && isChildExpanded && grandchildren.map((grandchild: any, k: number) => {
                                        const grandchildName = grandchild.path.split('/').pop() || grandchild.path
                                        return (
                                            <div key={`${i}-${j}-${k}`} className="grid grid-cols-12 gap-4 p-4 pl-32 hover:bg-secondary/30 transition-colors items-center group bg-secondary/20">
                                                <div className="col-span-5 flex items-center gap-3">
                                                    <div className="p-2 rounded-lg bg-purple-500/10 text-purple-500">
                                                        <File className="w-4 h-4" />
                                                    </div>
                                                    <div>
                                                        <div className="font-medium text-sm">{grandchildName}</div>
                                                    </div>
                                                </div>
                                                <div className="col-span-3 text-sm text-muted-foreground truncate hidden lg:block" title={grandchild.path}>{grandchild.path}</div>
                                                <div className="col-span-2 text-sm text-muted-foreground font-mono">{formatSize(grandchild.size)}</div>
                                                <div className="col-span-2 flex items-center justify-end gap-4">
                                                    <span className="text-xs px-2 py-1 rounded-full bg-blue-500/10 text-blue-500">
                                                        {grandchild.syncStatus}
                                                    </span>
                                                </div>
                                            </div>
                                        )
                                    })}
                                </div>
                            )
                        })}
                    </div>
                )
            })}
        </div>
      </div>
    </div>
  )
}
