import React, { useState, useEffect } from 'react';
// import { Card, CardContent, CardDescription, CardHeader, CardTitle } from './ui/card';
// import { Button } from './ui/button';
// import { Badge } from './ui/badge';
// import { ScrollArea } from './ui/scroll-area';
// import { Separator } from './ui/separator';
// import { Alert, AlertDescription } from './ui/alert';
import { 
  ChevronLeft, 
  ChevronRight, 
  Check, 
  X, 
  GitMerge, 
  FileText,
  Clock,
  User,
  AlertTriangle
} from 'lucide-react';

interface ConflictInfo {
  id: string;
  filePath: string;
  conflictType: 'CONTENT' | 'METADATA' | 'DELETION' | 'RENAME';
  localVersion: {
    version: number;
    hash: string;
    modifiedTime: number;
    deviceId: string;
    content?: string;
  };
  remoteVersion: {
    version: number;
    hash: string;
    modifiedTime: number;
    deviceId: string;
    content?: string;
  };
  baseVersion?: {
    version: number;
    hash: string;
    content?: string;
  };
  status: 'pending' | 'resolved' | 'ignored';
  conflicts: Array<{
    startLine: number;
    endLine: number;
    localContent: string;
    remoteContent: string;
    description: string;
  }>;
  resolutionStrategy?: 'local_wins' | 'remote_wins' | 'merge' | 'manual';
}

interface ConflictResolverProps {
  conflicts: ConflictInfo[];
  onResolve: (conflictId: string, resolution: {
    strategy: string;
    mergedContent?: string;
  }) => void;
  onIgnore: (conflictId: string) => void;
}

export const ConflictResolver: React.FC<ConflictResolverProps> = ({
  conflicts,
  onResolve,
  onIgnore
}) => {
  const [selectedConflict, setSelectedConflict] = useState<ConflictInfo | null>(null);
  const [currentConflictIndex, setCurrentConflictIndex] = useState(0);
  const [resolutionStrategy, setResolutionStrategy] = useState<string>('auto_merge');
  const [mergedContent, setMergedContent] = useState<string>('');
  const [isResolving, setIsResolving] = useState(false);

  const pendingConflicts = conflicts.filter(c => c.status === 'pending');
  const currentConflict = pendingConflicts[currentConflictIndex] || selectedConflict;

  useEffect(() => {
    if (currentConflict && currentConflict.conflicts.length > 0) {
      // Initialize with auto-merged content if available
      setMergedContent(generateInitialMerge(currentConflict));
    }
  }, [currentConflict]);

  const generateInitialMerge = (conflict: ConflictInfo): string => {
    // Simple initial merge - in real implementation, this would come from the backend
    const lines = conflict.localVersion.content?.split('\n') || [];
    return lines.join('\n');
  };

  const handleResolve = async () => {
    if (!currentConflict) return;

    setIsResolving(true);
    
    try {
      await onResolve(currentConflict.id, {
        strategy: resolutionStrategy,
        mergedContent: resolutionStrategy === 'manual' ? mergedContent : undefined
      });
      
      // Move to next conflict
      if (currentConflictIndex < pendingConflicts.length - 1) {
        setCurrentConflictIndex(currentConflictIndex + 1);
      } else {
        setSelectedConflict(null);
      }
    } catch (error) {
      console.error('Failed to resolve conflict:', error);
    } finally {
      setIsResolving(false);
    }
  };

  const handleIgnore = async () => {
    if (!currentConflict) return;
    
    await onIgnore(currentConflict.id);
    
    // Move to next conflict
    if (currentConflictIndex < pendingConflicts.length - 1) {
      setCurrentConflictIndex(currentConflictIndex + 1);
    } else {
      setSelectedConflict(null);
    }
  };

  const formatTimestamp = (timestamp: number): string => {
    return new Date(timestamp * 1000).toLocaleString();
  };

  const getConflictTypeColor = (type: string) => {
    switch (type) {
      case 'CONTENT': return 'destructive';
      case 'METADATA': return 'secondary';
      case 'DELETION': return 'outline';
      case 'RENAME': return 'default';
      default: return 'secondary';
    }
  };

  const renderDiffView = () => {
    if (!currentConflict) return null;

    return (
      <div className="grid grid-cols-3 gap-4 h-full">
        {/* Base Version */}
        {currentConflict.baseVersion && (
          <div className="border rounded-lg p-4 h-full bg-card">
            <div className="pb-2">
              <h3 className="text-sm flex items-center gap-2 font-semibold">
                <FileText className="w-4 h-4" />
                Base Version
                <span className="px-2 py-1 text-xs border rounded">v{currentConflict.baseVersion.version}</span>
              </h3>
            </div>
            <div>
              <div className="h-[400px] w-full rounded border p-4 overflow-auto">
                <pre className="text-sm whitespace-pre-wrap">
                  {currentConflict.baseVersion.content || 'No content'}
                </pre>
              </div>
            </div>
          </div>
        )}

        {/* Local Version */}
        <div className="border rounded-lg p-4 h-full bg-card">
          <div className="pb-2">
            <h3 className="text-sm flex items-center gap-2 font-semibold">
              <User className="w-4 h-4" />
              Local Changes
              <span className="px-2 py-1 text-xs border rounded">v{currentConflict.localVersion.version}</span>
            </h3>
            <p className="text-xs text-muted-foreground">
              Modified: {formatTimestamp(currentConflict.localVersion.modifiedTime)}
            </p>
          </div>
          <div>
            <div className="h-[400px] w-full rounded border p-4 overflow-auto">
              <pre className="text-sm whitespace-pre-wrap">
                {currentConflict.localVersion.content || 'No content'}
              </pre>
            </div>
          </div>
        </div>

        {/* Remote Version */}
        <div className="border rounded-lg p-4 h-full bg-card">
          <div className="pb-2">
            <h3 className="text-sm flex items-center gap-2 font-semibold">
              <User className="w-4 h-4" />
              Remote Changes
              <span className="px-2 py-1 text-xs border rounded">v{currentConflict.remoteVersion.version}</span>
            </h3>
            <p className="text-xs text-muted-foreground">
              Modified: {formatTimestamp(currentConflict.remoteVersion.modifiedTime)}
            </p>
          </div>
          <div>
            <div className="h-[400px] w-full rounded border p-4 overflow-auto">
              <pre className="text-sm whitespace-pre-wrap">
                {currentConflict.remoteVersion.content || 'No content'}
              </pre>
            </div>
          </div>
        </div>
      </div>
    );
  };

  const renderMergeEditor = () => {
    if (resolutionStrategy !== 'manual' || !currentConflict) return null;

    return (
      <div className="border rounded-lg p-4 mt-4 bg-card">
        <div>
          <h3 className="text-sm flex items-center gap-2 font-semibold">
            <GitMerge className="w-4 h-4" />
            Manual Merge Editor
          </h3>
          <p className="text-xs text-muted-foreground">
            Edit the merged content below. Conflict markers are shown for reference.
          </p>
        </div>
        <div>
          <div className="h-[300px] w-full rounded border overflow-auto">
            <textarea
              className="w-full h-full p-4 text-sm font-mono resize-none focus:outline-none"
              value={mergedContent}
              onChange={(e) => setMergedContent(e.target.value)}
              placeholder="Enter merged content..."
            />
          </div>
        </div>
      </div>
    );
  };

  if (pendingConflicts.length === 0) {
    return (
      <div className="border rounded-lg p-4 bg-card">
        <div className="flex flex-col items-center justify-center py-12">
          <Check className="w-12 h-12 text-green-500 mb-4" />
          <h3 className="text-lg font-semibold">No Conflicts</h3>
          <p className="text-muted-foreground text-center mt-2">
            All files are in sync. No conflicts to resolve.
          </p>
        </div>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      {/* Conflict List */}
      <div className="border rounded-lg p-4 bg-card">
        <div>
          <h3 className="flex items-center gap-2 font-semibold">
            <AlertTriangle className="w-5 h-5" />
            File Conflicts ({pendingConflicts.length})
          </h3>
          <p className="text-sm text-muted-foreground">
            Select a conflict to view details and resolve it
          </p>
        </div>
        <div>
          <div className="space-y-2">
            {pendingConflicts.map((conflict, index) => (
              <div
                key={conflict.id}
                className={`p-3 rounded-lg border cursor-pointer transition-colors ${
                  currentConflictIndex === index || selectedConflict?.id === conflict.id
                    ? 'border-primary bg-primary/5'
                    : 'border-border hover:bg-accent'
                }`}
                onClick={() => {
                  setSelectedConflict(conflict);
                  setCurrentConflictIndex(index);
                }}
              >
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-2">
                    <FileText className="w-4 h-4" />
                    <span className="font-medium">{conflict.filePath}</span>
                    <span className={`px-2 py-1 text-xs border rounded ${getConflictTypeColor(conflict.conflictType)}`}>
                      {conflict.conflictType}
                    </span>
                  </div>
                  <div className="flex items-center gap-2 text-sm text-muted-foreground">
                    <Clock className="w-3 h-3" />
                    {conflict.conflicts.length} conflict(s)
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Conflict Resolution */}
      {currentConflict && (
        <div className="border rounded-lg p-4 bg-card">
          <div>
            <div className="flex items-center justify-between">
              <div>
                <h3 className="flex items-center gap-2 font-semibold">
                  <GitMerge className="w-5 h-5" />
                  Resolve Conflict: {currentConflict.filePath}
                </h3>
                <p className="text-sm text-muted-foreground">
                  Conflict {currentConflictIndex + 1} of {pendingConflicts.length}
                </p>
              </div>
              <div className="flex items-center gap-2">
                <button
                  className="px-3 py-1 text-sm border rounded hover:bg-accent disabled:opacity-50"
                  onClick={() => setCurrentConflictIndex(Math.max(0, currentConflictIndex - 1))}
                  disabled={currentConflictIndex === 0}
                >
                  <ChevronLeft className="w-4 h-4" />
                </button>
                <button
                  className="px-3 py-1 text-sm border rounded hover:bg-accent disabled:opacity-50"
                  onClick={() => setCurrentConflictIndex(
                    Math.min(pendingConflicts.length - 1, currentConflictIndex + 1)
                  )}
                  disabled={currentConflictIndex === pendingConflicts.length - 1}
                >
                  <ChevronRight className="w-4 h-4" />
                </button>
              </div>
            </div>
          </div>
          <div className="space-y-4">
            {/* Resolution Strategy */}
            <div className="space-y-2">
              <label className="text-sm font-medium">Resolution Strategy</label>
              <select
                className="w-full p-2 border rounded-md"
                value={resolutionStrategy}
                onChange={(e) => setResolutionStrategy(e.target.value)}
              >
                <option value="auto_merge">Auto Merge (Recommended)</option>
                <option value="local_wins">Use Local Version</option>
                <option value="remote_wins">Use Remote Version</option>
                <option value="manual">Manual Merge</option>
              </select>
            </div>

            {/* Diff View */}
            {renderDiffView()}

            {/* Manual Merge Editor */}
            {renderMergeEditor()}

            {/* Actions */}
            <div className="flex items-center justify-between pt-4 border-t">
              <button
                className="px-4 py-2 text-sm border rounded hover:bg-accent disabled:opacity-50 flex items-center gap-2"
                onClick={handleIgnore}
                disabled={isResolving}
              >
                <X className="w-4 h-4" />
                Ignore Conflict
              </button>
              <button
                className="px-4 py-2 text-sm bg-primary text-primary-foreground rounded hover:bg-primary/90 disabled:opacity-50 flex items-center gap-2"
                onClick={handleResolve}
                disabled={isResolving}
              >
                <Check className="w-4 h-4" />
                {isResolving ? 'Resolving...' : 'Resolve Conflict'}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};
