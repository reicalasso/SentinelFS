import { File, HardDrive, RefreshCw } from 'lucide-react'

export function FilesListHeader() {
  return (
    <div className="files-list-header">
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
  )
}
