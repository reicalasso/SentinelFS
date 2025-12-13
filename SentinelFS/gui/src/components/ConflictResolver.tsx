import React, { useState, useEffect } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from './ui/card';
import { Button } from './ui/button';
import { Badge } from './ui/badge';
import { ScrollArea } from './ui/scroll-area';
import { Separator } from './ui/separator';
import { Alert, AlertDescription } from './ui/alert';
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
          <Card className="h-full">
            <CardHeader className="pb-2">
              <CardTitle className="text-sm flex items-center gap-2">
                <FileText className="w-4 h-4" />
                Base Version
                <Badge variant="outline">v{currentConflict.baseVersion.version}</Badge>
              </CardTitle>
            </CardHeader>
            <CardContent>
              <ScrollArea className="h-[400px] w-full rounded-md border p-4">
                <pre className="text-sm whitespace-pre-wrap">
                  {currentConflict.baseVersion.content || 'No content'}
                </pre>
              </ScrollArea>
            </CardContent>
          </Card>
        )}

        {/* Local Version */}
        <Card className="h-full">
          <CardHeader className="pb-2">
            <CardTitle className="text-sm flex items-center gap-2">
              <User className="w-4 h-4" />
              Local Changes
              <Badge variant="outline">v{currentConflict.localVersion.version}</Badge>
            </CardTitle>
            <CardDescription className="text-xs">
              Modified: {formatTimestamp(currentConflict.localVersion.modifiedTime)}
            </CardDescription>
          </CardHeader>
          <CardContent>
            <ScrollArea className="h-[400px] w-full rounded-md border p-4">
              <pre className="text-sm whitespace-pre-wrap">
                {currentConflict.localVersion.content || 'No content'}
              </pre>
            </ScrollArea>
          </CardContent>
        </Card>

        {/* Remote Version */}
        <Card className="h-full">
          <CardHeader className="pb-2">
            <CardTitle className="text-sm flex items-center gap-2">
              <User className="w-4 h-4" />
              Remote Changes
              <Badge variant="outline">v{currentConflict.remoteVersion.version}</Badge>
            </CardTitle>
            <CardDescription className="text-xs">
              Modified: {formatTimestamp(currentConflict.remoteVersion.modifiedTime)}
            </CardDescription>
          </CardHeader>
          <CardContent>
            <ScrollArea className="h-[400px] w-full rounded-md border p-4">
              <pre className="text-sm whitespace-pre-wrap">
                {currentConflict.remoteVersion.content || 'No content'}
              </pre>
            </ScrollArea>
          </CardContent>
        </Card>
      </div>
    );
  };

  const renderMergeEditor = () => {
    if (resolutionStrategy !== 'manual' || !currentConflict) return null;

    return (
      <Card className="mt-4">
        <CardHeader>
          <CardTitle className="text-sm flex items-center gap-2">
            <GitMerge className="w-4 h-4" />
            Manual Merge Editor
          </CardTitle>
          <CardDescription>
            Edit the merged content below. Conflict markers are shown for reference.
          </CardDescription>
        </CardHeader>
        <CardContent>
          <ScrollArea className="h-[300px] w-full rounded-md border">
            <textarea
              className="w-full h-full p-4 text-sm font-mono resize-none focus:outline-none"
              value={mergedContent}
              onChange={(e) => setMergedContent(e.target.value)}
              placeholder="Enter merged content..."
            />
          </ScrollArea>
        </CardContent>
      </Card>
    );
  };

  if (pendingConflicts.length === 0) {
    return (
      <Card>
        <CardContent className="flex flex-col items-center justify-center py-12">
          <Check className="w-12 h-12 text-green-500 mb-4" />
          <h3 className="text-lg font-semibold">No Conflicts</h3>
          <p className="text-muted-foreground text-center mt-2">
            All files are in sync. No conflicts to resolve.
          </p>
        </CardContent>
      </Card>
    );
  }

  return (
    <div className="space-y-4">
      {/* Conflict List */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <AlertTriangle className="w-5 h-5" />
            File Conflicts ({pendingConflicts.length})
          </CardTitle>
          <CardDescription>
            Select a conflict to view details and resolve it
          </CardDescription>
        </CardHeader>
        <CardContent>
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
                    <Badge variant={getConflictTypeColor(conflict.conflictType)}>
                      {conflict.conflictType}
                    </Badge>
                  </div>
                  <div className="flex items-center gap-2 text-sm text-muted-foreground">
                    <Clock className="w-3 h-3" />
                    {conflict.conflicts.length} conflict(s)
                  </div>
                </div>
              </div>
            ))}
          </div>
        </CardContent>
      </Card>

      {/* Conflict Resolution */}
      {currentConflict && (
        <Card>
          <CardHeader>
            <div className="flex items-center justify-between">
              <div>
                <CardTitle className="flex items-center gap-2">
                  <GitMerge className="w-5 h-5" />
                  Resolve Conflict: {currentConflict.filePath}
                </CardTitle>
                <CardDescription>
                  Conflict {currentConflictIndex + 1} of {pendingConflicts.length}
                </CardDescription>
              </div>
              <div className="flex items-center gap-2">
                <Button
                  variant="outline"
                  size="sm"
                  onClick={() => setCurrentConflictIndex(Math.max(0, currentConflictIndex - 1))}
                  disabled={currentConflictIndex === 0}
                >
                  <ChevronLeft className="w-4 h-4" />
                </Button>
                <Button
                  variant="outline"
                  size="sm"
                  onClick={() => setCurrentConflictIndex(
                    Math.min(pendingConflicts.length - 1, currentConflictIndex + 1)
                  )}
                  disabled={currentConflictIndex === pendingConflicts.length - 1}
                >
                  <ChevronRight className="w-4 h-4" />
                </Button>
              </div>
            </div>
          </CardHeader>
          <CardContent className="space-y-4">
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
              <Button
                variant="outline"
                onClick={handleIgnore}
                disabled={isResolving}
              >
                <X className="w-4 h-4 mr-2" />
                Ignore Conflict
              </Button>
              <Button
                onClick={handleResolve}
                disabled={isResolving}
              >
                <Check className="w-4 h-4 mr-2" />
                {isResolving ? 'Resolving...' : 'Resolve Conflict'}
              </Button>
            </div>
          </CardContent>
        </Card>
      )}
    </div>
  );
};
