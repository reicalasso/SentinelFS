import { useState, useEffect, useMemo, useRef } from 'react'
import { 
  Terminal, 
  Search, 
  Filter, 
  Download, 
  Trash2, 
  Pause, 
  Play, 
  ChevronDown, 
  ChevronUp,
  X,
  AlertTriangle,
  Info,
  Bug,
  AlertOctagon,
  Clock,
  Layers
} from 'lucide-react'

interface LogEntry {
  id: string
  raw: string
  timestamp: string
  level: 'DEBUG' | 'INFO' | 'WARN' | 'ERROR' | 'CRITICAL' | 'UNKNOWN'
  component: string
  message: string
}

interface LoggerProps {
  logs: string[]
  onClear: () => void
}

export function Logger({ logs, onClear }: LoggerProps) {
  const [searchTerm, setSearchTerm] = useState('')
  const [selectedLevel, setSelectedLevel] = useState<string>('INFO')
  const [autoScroll, setAutoScroll] = useState(true)
  const [isPaused, setIsPaused] = useState(false)
  const [expandedLogId, setExpandedLogId] = useState<string | null>(null)
  
  const scrollRef = useRef<HTMLDivElement>(null)
  const logsEndRef = useRef<HTMLDivElement>(null)

  // Parse logs into structured data
  const parsedLogs = useMemo(() => {
    return logs.map((log, index) => {
      // Regex to parse: [TIMESTAMP] [LEVEL] [COMPONENT] MESSAGE
      // Example: [2023-10-27 10:00:00] [INFO] [Network] Connection established
      const match = log.match(/^\[(.*?)\]\s*\[(.*?)\]\s*\[(.*?)\]\s*(.*)$/)
      
      if (match) {
        return {
          id: `log-${index}`,
          raw: log,
          timestamp: match[1],
          level: match[2].trim() as any,
          component: match[3].trim(),
          message: match[4]
        }
      }

      // Fallback for simple messages or different formats
      const simpleMatch = log.match(/^\[(.*?)\]\s*(.*)$/)
      return {
        id: `log-${index}`,
        raw: log,
        timestamp: simpleMatch ? simpleMatch[1] : new Date().toLocaleTimeString(),
        level: (log.includes('ERROR') ? 'ERROR' : log.includes('WARN') ? 'WARN' : 'INFO') as any,
        component: 'System',
        message: simpleMatch ? simpleMatch[2] : log
      }
    })
  }, [logs])

  // Filter logs
  const filteredLogs = useMemo(() => {
    return parsedLogs.filter(log => {
      const matchesSearch = 
        log.message.toLowerCase().includes(searchTerm.toLowerCase()) ||
        log.component.toLowerCase().includes(searchTerm.toLowerCase())
      
      const matchesLevel = selectedLevel === 'ALL' || log.level === selectedLevel
      
      return matchesSearch && matchesLevel
    })
  }, [parsedLogs, searchTerm, selectedLevel])

  // Auto-scroll logic
  useEffect(() => {
    if (autoScroll && !isPaused && logsEndRef.current) {
      logsEndRef.current.scrollIntoView({ behavior: 'smooth' })
    }
  }, [filteredLogs, autoScroll, isPaused])

  const handleExport = () => {
    try {
      const text = logs.join('\n')
      const blob = new Blob([text], { type: 'text/plain;charset=utf-8' })
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `sentinel_daemon_${new Date().toISOString().replace(/[:.]/g, '-')}.log`
      document.body.appendChild(a)
      a.click()
      document.body.removeChild(a)
      URL.revokeObjectURL(url)
    } catch (e) {
      console.error('Failed to export logs', e)
    }
  }

  const getLevelColor = (level: string) => {
    switch (level) {
      case 'ERROR':
      case 'CRITICAL':
        return 'text-red-500 bg-red-500/10 border-red-500/20'
      case 'WARN':
        return 'text-yellow-500 bg-yellow-500/10 border-yellow-500/20'
      case 'DEBUG':
        return 'text-blue-500 bg-blue-500/10 border-blue-500/20'
      case 'INFO':
        return 'text-green-500 bg-green-500/10 border-green-500/20'
      default:
        return 'text-gray-500 bg-gray-500/10 border-gray-500/20'
    }
  }

  const getLevelIcon = (level: string) => {
    switch (level) {
      case 'ERROR':
      case 'CRITICAL':
        return <AlertOctagon className="w-3.5 h-3.5" />
      case 'WARN':
        return <AlertTriangle className="w-3.5 h-3.5" />
      case 'DEBUG':
        return <Bug className="w-3.5 h-3.5" />
      case 'INFO':
      default:
        return <Info className="w-3.5 h-3.5" />
    }
  }

  return (
    <div className="flex flex-col h-full max-h-[80vh] bg-background rounded-xl border border-border shadow-2xl overflow-hidden">
      {/* Header Toolbar */}
      <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between p-4 gap-4 border-b border-border/30 bg-secondary/20">
        <div className="flex items-center gap-3">
          <div className="p-2.5 rounded-xl bg-gradient-to-br from-primary/20 to-primary/5 border border-primary/20">
            <Terminal className="w-5 h-5 text-primary" />
          </div>
          <div>
            <h2 className="text-lg font-bold flex items-center gap-2">
              System Logs
              <span className="px-2 py-0.5 rounded-full bg-primary/10 text-primary text-[10px] font-mono border border-primary/20">
                {filteredLogs.length}
              </span>
            </h2>
            <p className="text-xs text-muted-foreground">Real-time daemon activity monitoring</p>
          </div>
        </div>

        <div className="flex items-center gap-2 w-full sm:w-auto overflow-x-auto pb-1 sm:pb-0 no-scrollbar">
          {/* Controls Group */}
          <div className="flex items-center gap-1 p-1 rounded-lg bg-background/50 border border-border/30">
            <button
              onClick={() => setIsPaused(!isPaused)}
              className={`p-2 rounded-md transition-all ${
                isPaused 
                  ? 'bg-yellow-500/20 text-yellow-500 hover:bg-yellow-500/30' 
                  : 'hover:bg-secondary text-muted-foreground'
              }`}
              title={isPaused ? "Resume Auto-scroll" : "Pause Auto-scroll"}
            >
              {isPaused ? <Play className="w-4 h-4" /> : <Pause className="w-4 h-4" />}
            </button>
            <button
              onClick={onClear}
              className="p-2 rounded-md hover:bg-destructive/10 text-muted-foreground hover:text-destructive transition-all"
              title="Clear Logs"
            >
              <Trash2 className="w-4 h-4" />
            </button>
            <button
              onClick={handleExport}
              className="p-2 rounded-md hover:bg-secondary text-muted-foreground hover:text-foreground transition-all"
              title="Export Logs"
            >
              <Download className="w-4 h-4" />
            </button>
          </div>

          {/* Filter Group */}
          <div className="flex items-center gap-2">
            <div className="relative group">
              <Search className="w-4 h-4 absolute left-3 top-1/2 -translate-y-1/2 text-muted-foreground group-focus-within:text-primary transition-colors" />
              <input 
                type="text" 
                placeholder="Search logs..." 
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="pl-9 pr-4 py-1.5 h-9 w-40 sm:w-64 bg-background/50 border border-border/30 rounded-lg text-sm focus:outline-none focus:ring-2 focus:ring-primary/20 focus:border-primary/50 transition-all"
              />
              {searchTerm && (
                <button 
                  onClick={() => setSearchTerm('')}
                  className="absolute right-2 top-1/2 -translate-y-1/2 p-0.5 hover:bg-secondary rounded-full"
                >
                  <X className="w-3 h-3 text-muted-foreground" />
                </button>
              )}
            </div>

            <select 
              value={selectedLevel}
              onChange={(e) => setSelectedLevel(e.target.value)}
              className="h-9 px-3 bg-background/50 border border-border/30 rounded-lg text-sm focus:outline-none focus:ring-2 focus:ring-primary/20 focus:border-primary/50 cursor-pointer"
            >
              <option value="ALL">All Levels</option>
              <option value="INFO">Info</option>
              <option value="WARN">Warnings</option>
              <option value="ERROR">Errors</option>
              <option value="DEBUG">Debug</option>
            </select>
          </div>
        </div>
      </div>

      {/* Logs Content */}
      <div 
        ref={scrollRef}
        className="flex-1 overflow-y-auto overflow-x-hidden p-2 sm:p-4 font-mono text-xs space-y-1 bg-[#0d1117]/80 custom-scrollbar"
      >
        {filteredLogs.length === 0 ? (
          <div className="h-full flex flex-col items-center justify-center text-muted-foreground/40 gap-4">
            <div className="p-4 rounded-full bg-secondary/30">
              <Terminal className="w-8 h-8" />
            </div>
            <p>No logs found matching your criteria</p>
          </div>
        ) : (
          filteredLogs.map((log) => (
            <div 
              key={log.id}
              className={`group relative flex flex-col sm:flex-row sm:items-start gap-2 sm:gap-4 p-2 sm:px-4 sm:py-2 rounded-lg border border-transparent hover:border-border/30 hover:bg-white/[0.03] transition-all ${
                expandedLogId === log.id ? 'bg-white/[0.05] border-border/40' : ''
              }`}
            >
              {/* Left: Metadata */}
              <div className="flex sm:flex-col items-center sm:items-start gap-2 sm:gap-1 sm:w-32 flex-shrink-0 select-none opacity-70 group-hover:opacity-100 transition-opacity">
                <span className="text-[10px] text-muted-foreground flex items-center gap-1.5">
                    <Clock className="w-3 h-3" />
                    {log.timestamp.split(' ')[1] || log.timestamp}
                </span>
                <div className={`flex items-center gap-1.5 px-1.5 py-0.5 rounded-md border text-[10px] font-bold ${getLevelColor(log.level)}`}>
                  {getLevelIcon(log.level)}
                  {log.level}
                </div>
              </div>

              {/* Middle: Component & Message */}
              <div className="flex-1 min-w-0">
                <div className="flex items-center gap-2 mb-0.5">
                   <div className="flex items-center gap-1.5 px-2 py-0.5 rounded-md bg-secondary/40 text-secondary-foreground text-[10px] font-medium border border-border/20">
                      <Layers className="w-3 h-3 opacity-70" />
                      {log.component}
                   </div>
                </div>
                <div 
                  className={`break-all text-sm/relaxed ${
                    log.level === 'ERROR' || log.level === 'CRITICAL' ? 'text-red-400' :
                    log.level === 'WARN' ? 'text-yellow-400' :
                    'text-foreground/90'
                  } ${expandedLogId === log.id ? '' : 'line-clamp-2'}`}
                  onClick={() => setExpandedLogId(expandedLogId === log.id ? null : log.id)}
                >
                  {log.message}
                </div>
              </div>

              {/* Right: Expand/Actions */}
              <div className="hidden group-hover:flex items-center gap-2 absolute right-2 top-2">
                <button 
                   onClick={() => setExpandedLogId(expandedLogId === log.id ? null : log.id)}
                   className="p-1 rounded hover:bg-secondary text-muted-foreground hover:text-foreground transition-colors"
                >
                   {expandedLogId === log.id ? <ChevronUp className="w-4 h-4" /> : <ChevronDown className="w-4 h-4" />}
                </button>
              </div>
            </div>
          ))
        )}
        <div ref={logsEndRef} />
      </div>

      {/* Footer Status */}
      <div className="px-4 py-2 bg-secondary/20 border-t border-border/30 text-[10px] text-muted-foreground flex items-center justify-between">
         <div className="flex items-center gap-2">
            <div className={`w-2 h-2 rounded-full ${isPaused ? 'bg-yellow-500 animate-pulse' : 'bg-green-500 animate-pulse'}`}></div>
            {isPaused ? 'Auto-scroll Paused' : 'Live Monitoring'}
         </div>
         <div className="font-mono opacity-50">
            Memory: {Math.round(logs.length * 0.5)}KB (approx)
         </div>
      </div>
    </div>
  )
}
