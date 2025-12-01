import { ArrowDown, ArrowUp, CheckCircle2, Pause, Play, X, Download, Upload, Activity, ArrowRightLeft, Clock } from 'lucide-react'

export function Transfers({ metrics, transfers, history }: { metrics?: any, transfers?: any[], history?: any[] }) {
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
  const activeTransfers = transfers && transfers.length > 0 ? transfers : []

  return (
    <div className="space-y-6 animate-in fade-in duration-500 slide-in-from-bottom-4">
        <div className="flex items-center justify-between">
            <div>
                <h2 className="text-lg font-semibold flex items-center gap-2">
                    <ArrowRightLeft className="w-5 h-5 text-primary" />
                    Transfer Activity
                </h2>
                <p className="text-sm text-muted-foreground">Monitor file sync progress and history</p>
            </div>
        </div>

        {/* Transfer Statistics */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
            <div className="bg-card/50 backdrop-blur-sm border border-border/50 rounded-xl p-5 shadow-sm hover:border-emerald-500/20 transition-colors group">
                <div className="flex items-center gap-4">
                    <div className="p-3 rounded-xl bg-emerald-500/10 group-hover:bg-emerald-500/20 transition-colors text-emerald-500">
                        <Download className="w-6 h-6" />
                    </div>
                    <div>
                        <p className="text-sm text-muted-foreground font-medium">Total Downloaded</p>
                        <p className="text-2xl font-bold tracking-tight">{formatSize(totalDownloaded)}</p>
                    </div>
                </div>
            </div>
            <div className="bg-card/50 backdrop-blur-sm border border-border/50 rounded-xl p-5 shadow-sm hover:border-blue-500/20 transition-colors group">
                <div className="flex items-center gap-4">
                    <div className="p-3 rounded-xl bg-blue-500/10 group-hover:bg-blue-500/20 transition-colors text-blue-500">
                        <Upload className="w-6 h-6" />
                    </div>
                    <div>
                        <p className="text-sm text-muted-foreground font-medium">Total Uploaded</p>
                        <p className="text-2xl font-bold tracking-tight">{formatSize(totalUploaded)}</p>
                    </div>
                </div>
            </div>
            <div className="bg-card/50 backdrop-blur-sm border border-border/50 rounded-xl p-5 shadow-sm hover:border-violet-500/20 transition-colors group">
                <div className="flex items-center gap-4">
                    <div className="p-3 rounded-xl bg-violet-500/10 group-hover:bg-violet-500/20 transition-colors text-violet-500">
                        <Activity className="w-6 h-6" />
                    </div>
                    <div>
                        <p className="text-sm text-muted-foreground font-medium">Files Synced</p>
                        <p className="text-2xl font-bold tracking-tight">{filesSynced}</p>
                    </div>
                </div>
            </div>
        </div>

        {/* Active Transfers */}
        <div className="space-y-4">
            <h3 className="font-semibold text-sm text-muted-foreground uppercase tracking-wider">Active Transfers</h3>
            {activeTransfers.length === 0 && (
                <div className="bg-card/30 border border-dashed border-border/50 rounded-xl p-8 text-center text-muted-foreground flex flex-col items-center">
                    <div className="bg-secondary/50 p-3 rounded-full mb-3">
                        <CheckCircle2 className="w-6 h-6 opacity-50" />
                    </div>
                    <p>All files are up to date</p>
                </div>
            )}
            {activeTransfers.map((transfer: any, i: number) => (
                <div key={i} className="bg-card border border-border rounded-xl p-4 shadow-sm">
                    <div className="flex items-center justify-between mb-2">
                        <div className="flex items-center gap-3">
                            <div className={`p-2 rounded-lg ${transfer.type === 'download' ? 'bg-emerald-500/10 text-emerald-500' : 'bg-blue-500/10 text-blue-500'}`}>
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
                            className={`h-full rounded-full transition-all duration-500 ${transfer.type === 'download' ? 'bg-emerald-500' : 'bg-blue-500'}`} 
                            style={{ width: `${transfer.progress}%` }}
                        ></div>
                    </div>
                </div>
            ))}
        </div>

        {/* History */}
        <div>
            <h3 className="font-semibold text-sm text-muted-foreground uppercase tracking-wider mb-4 mt-8">Recent History</h3>
            {(!history || history.length === 0) ? (
                <div className="bg-card/30 border border-border/50 rounded-xl overflow-hidden text-center text-muted-foreground p-6">
                    <p className="text-sm opacity-70">No recent transfer history</p>
                </div>
            ) : (
                <div className="bg-card/50 backdrop-blur-sm border border-border/50 rounded-xl overflow-hidden">
                    <div className="divide-y divide-border/30">
                        {history.map((item: any, i: number) => (
                            <div key={i} className="flex items-center justify-between p-4 hover:bg-secondary/30 transition-colors">
                                <div className="flex items-center gap-3">
                                    <div className={`p-2 rounded-lg ${
                                        item.type === 'download' || item.type === 'pull' 
                                            ? 'bg-emerald-500/10 text-emerald-500' 
                                            : item.type === 'delete' 
                                            ? 'bg-red-500/10 text-red-500'
                                            : 'bg-blue-500/10 text-blue-500'
                                    }`}>
                                        {item.type === 'download' || item.type === 'pull' 
                                            ? <ArrowDown className="w-4 h-4" /> 
                                            : item.type === 'delete'
                                            ? <X className="w-4 h-4" />
                                            : <ArrowUp className="w-4 h-4" />
                                        }
                                    </div>
                                    <div>
                                        <div className="font-medium text-sm">{item.file}</div>
                                        <div className="text-xs text-muted-foreground capitalize">{item.type}</div>
                                    </div>
                                </div>
                                <div className="flex items-center gap-2 text-xs text-muted-foreground">
                                    <Clock className="w-3 h-3" />
                                    <span>{item.time}</span>
                                </div>
                            </div>
                        ))}
                    </div>
                </div>
            )}
        </div>
    </div>
  )
}
