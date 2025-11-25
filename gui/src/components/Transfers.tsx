import { ArrowDown, ArrowUp, CheckCircle2, Pause, Play, X, Download, Upload, Activity } from 'lucide-react'

const activeTransfers = [
    { file: 'Backup_2024.zip', size: '4.2 GB', progress: 45, speed: '12 MB/s', type: 'download', peer: 'MacBook Pro' },
    { file: 'video_render_final.mov', size: '850 MB', progress: 78, speed: '8.5 MB/s', type: 'upload', peer: 'Ubuntu Server' },
]

const transferHistory = [
    { file: 'notes.txt', size: '2 KB', time: '2 mins ago', status: 'Completed', type: 'download' },
    { file: 'presentation.pptx', size: '14 MB', time: '15 mins ago', status: 'Completed', type: 'upload' },
    { file: 'dataset_large.csv', size: '1.2 GB', time: '1 hour ago', status: 'Failed', type: 'download' },
]

export function Transfers({ metrics }: { metrics?: any }) {
  const formatSize = (bytes: number) => {
    if (!bytes) return '0 B'
    if (bytes < 1024) return bytes + ' B'
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
    if (bytes < 1024 * 1024 * 1024) return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
    return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
  }
  
  const totalUploaded = metrics?.totalUploaded || 0
  const totalDownloaded = metrics?.totalDownloaded || 0
  const filesSynced = metrics?.filesSynced || 0

  return (
    <div className="space-y-6 animate-in fade-in duration-500">
        <div className="flex items-center justify-between">
            <h2 className="text-lg font-semibold">Transfer Activity</h2>
        </div>

        {/* Transfer Statistics */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            <div className="bg-card border border-border rounded-xl p-4 shadow-sm">
                <div className="flex items-center gap-3">
                    <div className="p-2 rounded-lg bg-green-500/10">
                        <Download className="w-5 h-5 text-green-500" />
                    </div>
                    <div>
                        <p className="text-sm text-muted-foreground">Total Downloaded</p>
                        <p className="text-xl font-semibold">{formatSize(totalDownloaded)}</p>
                    </div>
                </div>
            </div>
            <div className="bg-card border border-border rounded-xl p-4 shadow-sm">
                <div className="flex items-center gap-3">
                    <div className="p-2 rounded-lg bg-blue-500/10">
                        <Upload className="w-5 h-5 text-blue-500" />
                    </div>
                    <div>
                        <p className="text-sm text-muted-foreground">Total Uploaded</p>
                        <p className="text-xl font-semibold">{formatSize(totalUploaded)}</p>
                    </div>
                </div>
            </div>
            <div className="bg-card border border-border rounded-xl p-4 shadow-sm">
                <div className="flex items-center gap-3">
                    <div className="p-2 rounded-lg bg-purple-500/10">
                        <Activity className="w-5 h-5 text-purple-500" />
                    </div>
                    <div>
                        <p className="text-sm text-muted-foreground">Files Synced</p>
                        <p className="text-xl font-semibold">{filesSynced}</p>
                    </div>
                </div>
            </div>
        </div>

        {/* Active Transfers */}
        <div className="space-y-4">
            {activeTransfers.map((transfer, i) => (
                <div key={i} className="bg-card border border-border rounded-xl p-4 shadow-sm">
                    <div className="flex items-center justify-between mb-2">
                        <div className="flex items-center gap-3">
                            <div className={`p-2 rounded-lg ${transfer.type === 'download' ? 'bg-green-500/10 text-green-500' : 'bg-blue-500/10 text-blue-500'}`}>
                                {transfer.type === 'download' ? <ArrowDown className="w-4 h-4" /> : <ArrowUp className="w-4 h-4" />}
                            </div>
                            <div>
                                <div className="font-medium text-sm">{transfer.file}</div>
                                <div className="text-xs text-muted-foreground flex gap-2">
                                    <span>{transfer.type === 'download' ? 'Downloading from' : 'Uploading to'} {transfer.peer}</span>
                                    <span>•</span>
                                    <span>{transfer.size}</span>
                                </div>
                            </div>
                        </div>
                        <div className="flex items-center gap-4">
                            <div className="text-right">
                                <div className="font-mono text-sm">{transfer.speed}</div>
                                <div className="text-xs text-muted-foreground">{transfer.progress}%</div>
                            </div>
                            <div className="flex gap-1">
                                <button className="p-1 hover:bg-secondary rounded text-muted-foreground hover:text-foreground">
                                    <Pause className="w-4 h-4" />
                                </button>
                                <button className="p-1 hover:bg-secondary rounded text-muted-foreground hover:text-red-500">
                                    <X className="w-4 h-4" />
                                </button>
                            </div>
                        </div>
                    </div>
                    {/* Progress Bar */}
                    <div className="h-1.5 w-full bg-secondary rounded-full overflow-hidden">
                        <div 
                            className={`h-full rounded-full transition-all duration-500 ${transfer.type === 'download' ? 'bg-green-500' : 'bg-blue-500'}`} 
                            style={{ width: `${transfer.progress}%` }}
                        ></div>
                    </div>
                </div>
            ))}
        </div>

        {/* History */}
        <div>
            <h3 className="font-semibold text-sm text-muted-foreground uppercase tracking-wider mb-4 mt-8">Recent History</h3>
            <div className="bg-card border border-border rounded-xl overflow-hidden">
                <div className="divide-y divide-border">
                    {transferHistory.map((item, i) => (
                        <div key={i} className="p-4 flex items-center justify-between hover:bg-secondary/30 transition-colors">
                            <div className="flex items-center gap-3">
                                {item.status === 'Completed' ? (
                                    <CheckCircle2 className="w-4 h-4 text-green-500" />
                                ) : (
                                    <X className="w-4 h-4 text-red-500" />
                                )}
                                <div>
                                    <div className="font-medium text-sm">{item.file}</div>
                                    <div className="text-xs text-muted-foreground">{item.size} • {item.type}</div>
                                </div>
                            </div>
                            <div className="text-xs text-muted-foreground">{item.time}</div>
                        </div>
                    ))}
                </div>
            </div>
        </div>
    </div>
  )
}
