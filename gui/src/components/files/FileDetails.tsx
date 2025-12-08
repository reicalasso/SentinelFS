import { useState } from 'react'
import { 
  X, 
  FileText, 
  Lock, 
  Unlock, 
  Hash, 
  Calendar, 
  User, 
  Shield,
  Tag,
  Plus,
  Trash2,
  Copy,
  Check,
  AlertCircle,
  Link2
} from 'lucide-react'

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

interface FileDetailsProps {
  file: FileInfo | null
  onClose: () => void
  formatSize: (bytes: number) => string
}

export function FileDetails({ file, onClose, formatSize }: FileDetailsProps) {
  const [isLocked, setIsLocked] = useState(false)
  const [newXattrKey, setNewXattrKey] = useState('')
  const [newXattrValue, setNewXattrValue] = useState('')
  const [copiedHash, setCopiedHash] = useState(false)
  const [isAddingXattr, setIsAddingXattr] = useState(false)

  if (!file) return null

  const handleLockToggle = async () => {
    if (!window.api) return
    
    const command = isLocked ? `IRONROOT_UNLOCK ${file.path}` : `IRONROOT_LOCK ${file.path}`
    const response = await window.api.sendCommand(command)
    if (response.success) {
      setIsLocked(!isLocked)
    }
  }

  const handleCopyHash = () => {
    navigator.clipboard.writeText(file.hash)
    setCopiedHash(true)
    setTimeout(() => setCopiedHash(false), 2000)
  }

  const handleAddXattr = async () => {
    if (!window.api || !newXattrKey || !newXattrValue) return
    
    const response = await window.api.sendCommand(`IRONROOT_SET_XATTR ${file.path} ${newXattrKey} ${newXattrValue}`)
    if (response.success) {
      setNewXattrKey('')
      setNewXattrValue('')
      setIsAddingXattr(false)
    }
  }

  const handleRemoveXattr = async (key: string) => {
    if (!window.api) return
    
    const response = await window.api.sendCommand(`IRONROOT_REMOVE_XATTR ${file.path} ${key}`)
    if (response.success) {
      // Refresh would happen via parent
    }
  }

  const formatDate = (timestamp: number) => {
    return new Date(timestamp * 1000).toLocaleString()
  }

  const formatMode = (mode: number) => {
    const perms = ['---', '--x', '-w-', '-wx', 'r--', 'r-x', 'rw-', 'rwx']
    const owner = perms[(mode >> 6) & 7]
    const group = perms[(mode >> 3) & 7]
    const other = perms[mode & 7]
    return `${owner}${group}${other}`
  }

  return (
    <div className="fixed inset-0 bg-black/50 backdrop-blur-sm z-50 flex items-center justify-center p-4">
      <div className="bg-card border border-border rounded-2xl shadow-2xl w-full max-w-lg max-h-[90vh] overflow-hidden animate-in zoom-in-95 duration-200">
        {/* Header */}
        <div className="flex items-center justify-between p-4 border-b border-border bg-muted/30">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-orange-500/20 to-red-500/20 flex items-center justify-center">
              <FileText className="w-5 h-5 text-orange-400" />
            </div>
            <div>
              <h3 className="font-semibold text-foreground">File Details</h3>
              <p className="text-xs text-muted-foreground truncate max-w-[200px]">
                {file.path.split('/').pop()}
              </p>
            </div>
          </div>
          <button 
            onClick={onClose}
            className="p-2 rounded-lg hover:bg-muted transition-colors"
          >
            <X className="w-5 h-5 text-muted-foreground" />
          </button>
        </div>

        {/* Content */}
        <div className="p-4 space-y-4 overflow-y-auto max-h-[60vh]">
          {/* Symlink Warning */}
          {file.isSymlink && (
            <div className="flex items-center gap-2 p-3 rounded-lg bg-amber-500/10 border border-amber-500/20">
              <Link2 className="w-4 h-4 text-amber-400" />
              <div className="text-sm">
                <span className="text-amber-400 font-medium">Symbolic Link</span>
                <span className="text-muted-foreground"> → {file.symlinkTarget}</span>
              </div>
            </div>
          )}

          {/* Hash */}
          <div className="space-y-2">
            <label className="text-xs text-muted-foreground uppercase tracking-wider flex items-center gap-1.5">
              <Hash className="w-3 h-3" />
              SHA-256 Hash
            </label>
            <div className="flex items-center gap-2">
              <code className="flex-1 text-xs bg-muted/50 px-3 py-2 rounded-lg font-mono text-foreground truncate">
                {file.hash || 'N/A'}
              </code>
              <button 
                onClick={handleCopyHash}
                className="p-2 rounded-lg hover:bg-muted transition-colors"
                title="Copy hash"
              >
                {copiedHash ? (
                  <Check className="w-4 h-4 text-emerald-400" />
                ) : (
                  <Copy className="w-4 h-4 text-muted-foreground" />
                )}
              </button>
            </div>
          </div>

          {/* Info Grid */}
          <div className="grid grid-cols-2 gap-3">
            <div className="bg-muted/30 rounded-lg p-3">
              <div className="text-[10px] text-muted-foreground uppercase tracking-wider mb-1">Size</div>
              <div className="text-sm font-medium text-foreground">{formatSize(file.size)}</div>
            </div>
            <div className="bg-muted/30 rounded-lg p-3">
              <div className="text-[10px] text-muted-foreground uppercase tracking-wider mb-1">Modified</div>
              <div className="text-sm font-medium text-foreground">{formatDate(file.mtime)}</div>
            </div>
            <div className="bg-muted/30 rounded-lg p-3">
              <div className="text-[10px] text-muted-foreground uppercase tracking-wider mb-1">Permissions</div>
              <div className="text-sm font-mono text-foreground">{formatMode(file.mode)}</div>
            </div>
            <div className="bg-muted/30 rounded-lg p-3">
              <div className="text-[10px] text-muted-foreground uppercase tracking-wider mb-1">Owner</div>
              <div className="text-sm font-medium text-foreground">{file.uid}:{file.gid}</div>
            </div>
          </div>

          {/* Lock Control */}
          <div className="flex items-center justify-between p-3 rounded-lg bg-muted/30">
            <div className="flex items-center gap-2">
              {isLocked ? (
                <Lock className="w-4 h-4 text-amber-400" />
              ) : (
                <Unlock className="w-4 h-4 text-muted-foreground" />
              )}
              <span className="text-sm text-foreground">File Lock</span>
            </div>
            <button
              onClick={handleLockToggle}
              className={`px-3 py-1.5 rounded-lg text-xs font-medium transition-colors ${
                isLocked 
                  ? 'bg-amber-500/20 text-amber-400 hover:bg-amber-500/30' 
                  : 'bg-muted hover:bg-muted/80 text-muted-foreground'
              }`}
            >
              {isLocked ? 'Unlock' : 'Lock'}
            </button>
          </div>

          {/* Extended Attributes */}
          <div className="space-y-2">
            <div className="flex items-center justify-between">
              <label className="text-xs text-muted-foreground uppercase tracking-wider flex items-center gap-1.5">
                <Tag className="w-3 h-3" />
                Extended Attributes
              </label>
              <button
                onClick={() => setIsAddingXattr(true)}
                className="p-1 rounded hover:bg-muted transition-colors"
                title="Add attribute"
              >
                <Plus className="w-4 h-4 text-muted-foreground" />
              </button>
            </div>
            
            {isAddingXattr && (
              <div className="flex gap-2 p-2 rounded-lg bg-muted/30">
                <input
                  type="text"
                  placeholder="Key"
                  value={newXattrKey}
                  onChange={(e) => setNewXattrKey(e.target.value)}
                  className="flex-1 px-2 py-1 text-xs bg-background rounded border border-border focus:border-primary outline-none"
                />
                <input
                  type="text"
                  placeholder="Value"
                  value={newXattrValue}
                  onChange={(e) => setNewXattrValue(e.target.value)}
                  className="flex-1 px-2 py-1 text-xs bg-background rounded border border-border focus:border-primary outline-none"
                />
                <button
                  onClick={handleAddXattr}
                  className="px-2 py-1 text-xs bg-primary text-primary-foreground rounded hover:bg-primary/90"
                >
                  Add
                </button>
                <button
                  onClick={() => setIsAddingXattr(false)}
                  className="px-2 py-1 text-xs bg-muted text-muted-foreground rounded hover:bg-muted/80"
                >
                  Cancel
                </button>
              </div>
            )}

            <div className="space-y-1">
              {Object.entries(file.xattrs || {}).length === 0 ? (
                <div className="text-xs text-muted-foreground italic p-2">No extended attributes</div>
              ) : (
                Object.entries(file.xattrs).map(([key, value]) => (
                  <div key={key} className="flex items-center justify-between p-2 rounded-lg bg-muted/20 group">
                    <div className="flex-1 min-w-0">
                      <span className="text-xs font-mono text-blue-400">{key}</span>
                      <span className="text-xs text-muted-foreground mx-1">=</span>
                      <span className="text-xs font-mono text-foreground truncate">{value}</span>
                    </div>
                    <button
                      onClick={() => handleRemoveXattr(key)}
                      className="p-1 rounded opacity-0 group-hover:opacity-100 hover:bg-destructive/20 transition-all"
                      title="Remove attribute"
                    >
                      <Trash2 className="w-3 h-3 text-destructive" />
                    </button>
                  </div>
                ))
              )}
            </div>
          </div>
        </div>

        {/* Footer */}
        <div className="p-4 border-t border-border bg-muted/20">
          <div className="text-[10px] text-muted-foreground truncate">
            {file.path}
          </div>
        </div>
      </div>
    </div>
  )
}
