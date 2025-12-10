import { FolderOpen } from 'lucide-react'

interface FilesEmptyProps {
  onAddFolder: () => void
}

export function FilesEmpty({ onAddFolder }: FilesEmptyProps) {
  return (
    <div className="files-empty">
      <div className="files-empty-icon-wrapper">
        <div className="files-empty-glow"></div>
        <div className="files-empty-icon-box">
          <FolderOpen className="w-8 h-8 sm:w-12 sm:h-12 text-muted-foreground/30" />
        </div>
      </div>
      <h3 className="files-empty-title">No Synced Files Yet</h3>
      <p className="files-empty-desc">Add a folder to start synchronizing your files across devices.</p>
      <button onClick={onAddFolder} className="files-empty-btn">
        Add Your First Folder
      </button>
    </div>
  )
}
