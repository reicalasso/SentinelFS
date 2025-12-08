import { FolderOpen, Plus, Folder, File, HardDrive, Shield } from 'lucide-react'

interface FilesStats {
  totalFolders: number
  totalFiles: number
  totalSize: number
}

interface FilesHeroProps {
  stats: FilesStats
  onAddFolder: () => void
  formatSize: (bytes: number) => string
}

export function FilesHero({ stats, onAddFolder, formatSize }: FilesHeroProps) {
  return (
    <div className="files-hero">
      {/* Animated Background */}
      <div className="files-hero-bg">
        <div className="files-hero-orb files-hero-orb-1"></div>
        <div className="files-hero-orb files-hero-orb-2"></div>
        <div className="files-hero-pattern"></div>
      </div>
      
      <div className="files-hero-content">
        <div className="files-hero-header">
          <div className="files-hero-title-group">
            <div className="files-hero-icon">
              <FolderOpen className="w-6 h-6 sm:w-8 sm:h-8 status-success" />
            </div>
            <div>
              <h2 className="files-hero-title flex items-center gap-2">
                SYNC Files
                <span className="flex items-center gap-1 text-xs px-2 py-0.5 rounded-full bg-orange-500/20 text-orange-400 font-medium">
                  <Shield className="w-3 h-3" />
                  IronRoot
                </span>
              </h2>
              <p className="files-hero-subtitle">
                <span className="dot-success animate-pulse"></span>
                Advanced filesystem monitoring with debouncing & xattr
              </p>
            </div>
          </div>
          
          <button onClick={onAddFolder} className="files-add-btn">
            <div className="files-add-btn-shine"></div>
            <Plus className="w-4 h-4 relative" />
            <span className="relative">Add Folder</span>
          </button>
        </div>
        
        {/* Stats Row */}
        <div className="files-stats-grid">
          <div className="files-stat-card files-stat-folders">
            <div className="files-stat-icon">
              <Folder className="w-3 h-3 sm:w-4 sm:h-4 status-success" />
            </div>
            <div className="min-w-0">
              <div className="files-stat-value status-success">{stats.totalFolders}</div>
              <div className="files-stat-label">Folders</div>
            </div>
          </div>
          <div className="files-stat-card files-stat-files">
            <div className="files-stat-icon">
              <File className="w-3 h-3 sm:w-4 sm:h-4 text-accent" />
            </div>
            <div className="min-w-0">
              <div className="files-stat-value text-accent">{stats.totalFiles}</div>
              <div className="files-stat-label">Files</div>
            </div>
          </div>
          <div className="files-stat-card files-stat-size">
            <div className="files-stat-icon">
              <HardDrive className="w-3 h-3 sm:w-4 sm:h-4 text-primary" />
            </div>
            <div className="min-w-0">
              <div className="files-stat-value text-primary truncate">{formatSize(stats.totalSize)}</div>
              <div className="files-stat-label">Total</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
