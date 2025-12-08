import { AlertTriangle, FileWarning, Hash, HardDrive, Calendar, Brain, Activity, ShieldAlert, CheckCircle2 } from 'lucide-react'
import { DetectedThreat, formatSize, formatTime, getThreatTypeLabel, getThreatLevelColor, getThreatLevelBgColor } from './types'

interface ThreatDetailsProps {
  threat: DetectedThreat | null
}

export function ThreatDetails({ threat }: ThreatDetailsProps) {
  if (!threat) {
    return (
      <div className="flex items-center justify-center h-full">
        <div className="text-center p-8">
          <ShieldAlert className="w-16 h-16 text-muted-foreground/50 mx-auto mb-4" />
          <p className="text-muted-foreground">Select a threat to view details</p>
        </div>
      </div>
    )
  }

  const InfoRow = ({ icon, label, value, highlight = false }: { icon: React.ReactNode, label: string, value: string | number, highlight?: boolean }) => (
    <div className="flex items-start gap-3 p-3 bg-background/30 rounded-lg">
      <div className="mt-0.5 text-muted-foreground">{icon}</div>
      <div className="flex-1">
        <div className="text-xs text-muted-foreground mb-1">{label}</div>
        <div className={`text-sm font-medium ${highlight ? 'text-primary' : ''}`}>{value}</div>
      </div>
    </div>
  )

  return (
    <div className="h-full overflow-y-auto custom-scrollbar p-6 space-y-6">
      {/* Threat Status Banner */}
      <div className={`p-4 rounded-lg border ${getThreatLevelBgColor(threat.threatLevel)}`}>
        <div className="flex items-center gap-3 mb-2">
          {threat.markedSafe ? (
            <CheckCircle2 className="w-6 h-6 text-green-500" />
          ) : (
            <AlertTriangle className={`w-6 h-6 ${getThreatLevelColor(threat.threatLevel)}`} />
          )}
          <div>
            <div className="font-semibold text-lg">
              {threat.markedSafe ? 'Marked as Safe' : threat.threatLevel + ' Level Threat'}
            </div>
            <div className="text-sm text-muted-foreground">
              {getThreatTypeLabel(threat.threatType)}
            </div>
          </div>
        </div>
        {threat.markedSafe && (
          <div className="mt-2 text-sm text-muted-foreground">
            This file has been marked as safe but is still kept in quarantine.
          </div>
        )}
      </div>

      {/* File Information */}
      <div>
        <h3 className="text-sm font-semibold text-muted-foreground uppercase tracking-wide mb-3">
          File Information
        </h3>
        <div className="space-y-2">
          <InfoRow
            icon={<FileWarning className="w-4 h-4" />}
            label="File Path"
            value={threat.filePath}
            highlight
          />
          <InfoRow
            icon={<HardDrive className="w-4 h-4" />}
            label="File Size"
            value={formatSize(threat.fileSize)}
          />
          {threat.hash && (
            <InfoRow
              icon={<Hash className="w-4 h-4" />}
              label="SHA-256 Hash"
              value={threat.hash}
            />
          )}
          <InfoRow
            icon={<Calendar className="w-4 h-4" />}
            label="Detection Time"
            value={formatTime(threat.detectedAt)}
          />
        </div>
      </div>

      {/* Threat Analysis */}
      <div>
        <h3 className="text-sm font-semibold text-muted-foreground uppercase tracking-wide mb-3">
          Threat Analysis
        </h3>
        <div className="space-y-2">
          <InfoRow
            icon={<Activity className="w-4 h-4" />}
            label="Threat Score"
            value={`${threat.threatScore.toFixed(2)}%`}
          />
          {threat.entropy !== undefined && (
            <InfoRow
              icon={<Activity className="w-4 h-4" />}
              label="File Entropy"
              value={threat.entropy.toFixed(4)}
            />
          )}
          {threat.mlModelUsed && (
            <InfoRow
              icon={<Brain className="w-4 h-4" />}
              label="ML Model"
              value={threat.mlModelUsed}
            />
          )}
          {threat.quarantinePath && (
            <InfoRow
              icon={<ShieldAlert className="w-4 h-4" />}
              label="Quarantine Path"
              value={threat.quarantinePath}
            />
          )}
        </div>
      </div>

      {/* Additional Information */}
      {threat.additionalInfo && (
        <div>
          <h3 className="text-sm font-semibold text-muted-foreground uppercase tracking-wide mb-3">
            Additional Information
          </h3>
          <div className="p-4 bg-background/30 rounded-lg text-sm">
            {threat.additionalInfo}
          </div>
        </div>
      )}
    </div>
  )
}
