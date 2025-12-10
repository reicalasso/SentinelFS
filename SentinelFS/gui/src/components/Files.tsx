import { useState, useMemo } from 'react'
import { FilesHero, FilesSearch, FilesEmpty, FileTreeItem, FilesListHeader, FileDetails } from './files'

interface FileInfo {
  path: string
  hash: string
  size: number
  mtime: number
  mode: number
  uid: number
  gid: number
  isSymlink: boolean
  symlinkTarget?: string
  xattrs: Record<string, string>
}

export function Files({ files }: { files?: any[] }) {
  const [expandedFolders, setExpandedFolders] = useState<Set<string>>(new Set())
  const [searchQuery, setSearchQuery] = useState('')
  const [filterType, setFilterType] = useState('All Types')
  const [selectedFile, setSelectedFile] = useState<FileInfo | null>(null)
  
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

  const handleFileClick = async (item: any, e: React.MouseEvent) => {
    e.stopPropagation()
    if (item.isFolder) return
    if (!window.api) return
    
    try {
      const response = await window.api.sendCommand(`IRONROOT_FILE_INFO ${item.path}`) as any
      if (response.success && response.data) {
        setSelectedFile(response.data)
      }
    } catch (error) {
      console.error('Failed to get file info:', error)
    }
  }

  // Reconstruct the file tree from flat list
  const { tree: fileTree, flatList: flatItems } = useMemo(() => {
    if (!files) return { tree: [], flatList: [] }

    const map = new Map<string, any>()
    
    files.forEach(f => {
      map.set(f.path, { ...f, children: [] })
    })

    const getOrCreateFolder = (path: string, rootPath: string): any => {
      if (map.has(path)) return map.get(path)
      
      const newFolder = {
        path,
        name: path.split('/').pop(),
        isFolder: true,
        size: 0,
        syncStatus: 'synced',
        children: [],
      }
      map.set(path, newFolder)

      if (path !== rootPath && path.length > rootPath.length) {
        const lastSlash = path.lastIndexOf('/')
        if (lastSlash > 0) {
          const parentPath = path.substring(0, lastSlash)
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

    files.forEach(f => {
      if (f.isFolder && !f.parent) return

      const rootPath = f.parent
      if (!rootPath) return

      const lastSlash = f.path.lastIndexOf('/')
      const parentDir = f.path.substring(0, lastSlash)

      if (parentDir === rootPath) {
        const rootNode = map.get(rootPath)
        if (rootNode) {
          if (!rootNode.children.find((c: any) => c.path === f.path)) {
            rootNode.children.push(map.get(f.path))
          }
        }
      } else {
        const parentFolder = getOrCreateFolder(parentDir, rootPath)
        if (!parentFolder.children.find((c: any) => c.path === f.path)) {
          parentFolder.children.push(map.get(f.path))
        }
      }
    })

    const roots = Array.from(map.values()).filter(f => 
      f.isFolder && !f.parent && files.some(rf => rf.path === f.path && rf.isFolder && !rf.parent)
    )

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

  const isFiltering = searchQuery.length > 0 || filterType !== 'All Types'
  
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

  const stats = useMemo(() => {
    if (!flatItems) return { totalFiles: 0, totalFolders: 0, totalSize: 0 }
    const filesList = flatItems.filter((f: any) => !f.isFolder)
    const folders = flatItems.filter((f: any) => f.isFolder)
    const totalSize = filesList.reduce((acc: number, f: any) => acc + (f.size || 0), 0)
    return { totalFiles: filesList.length, totalFolders: folders.length, totalSize }
  }, [flatItems])

  return (
    <div className="space-y-4 sm:space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
      <FilesHero 
        stats={stats} 
        onAddFolder={handleAddFolder} 
        formatSize={formatSize} 
      />

      <FilesSearch 
        searchQuery={searchQuery}
        onSearchChange={setSearchQuery}
        filterType={filterType}
        onFilterChange={setFilterType}
      />

      <div className="card-modern overflow-hidden">
        <FilesListHeader />
        
        <div className="divide-y divide-border/20">
          {!hasRealData && <FilesEmpty onAddFolder={handleAddFolder} />}
          
          {displayItems.map((item: any) => (
            <FileTreeItem 
              key={item.path} 
              item={item} 
              level={0} 
              expandedFolders={expandedFolders} 
              toggleFolder={toggleFolder} 
              formatSize={formatSize} 
              handleRemoveWatch={handleRemoveWatch}
              isFiltering={isFiltering}
              onFileClick={handleFileClick}
            />
          ))}
        </div>
      </div>

      {/* File Details Modal */}
      {selectedFile && (
        <FileDetails 
          file={selectedFile} 
          onClose={() => setSelectedFile(null)} 
          formatSize={formatSize}
        />
      )}
    </div>
  )
}
