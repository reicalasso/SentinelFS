import { useState } from 'react'
import { ArrowRight, CheckCircle2, FolderPlus, KeyRound, X } from 'lucide-react'

interface OnboardingWizardProps {
  open: boolean
  onClose: () => void
  onCompleted?: () => void
}

export function OnboardingWizard({ open, onClose, onCompleted }: OnboardingWizardProps) {
  const [step, setStep] = useState(1)
  const [sessionCode, setSessionCode] = useState<string | null>(null)
  const [watchFolder, setWatchFolder] = useState<string | null>(null)
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState<string | null>(null)

  if (!open) return null

  const next = () => setStep(s => Math.min(3, s + 1))
  const back = () => setStep(s => Math.max(1, s - 1))

  const handleGenerateCode = async () => {
    if (!window.api) return
    setBusy(true)
    setError(null)
    try {
      const res = await window.api.sendCommand('GENERATE_CODE')
      if (!res.success) {
        setError(res.error ?? 'Failed to generate code')
        return
      }
      // Daemon returns new code in status, assumed here
      // UI tarafında sadece kullanıcıya akışı gösteriyoruz.
      setSessionCode('New code set')
      next()
    } finally {
      setBusy(false)
    }
  }

  const handleSelectFolder = async () => {
    if (!window.api) return
    setBusy(true)
    setError(null)
    try {
      const folder = await window.api.selectFolder()
      if (!folder) return
      const res = await window.api.sendCommand(`ADD_FOLDER ${folder}`)
      if (!res.success) {
        setError(res.error ?? 'Failed to add folder')
        return
      }
      setWatchFolder(folder)
      next()
    } finally {
      setBusy(false)
    }
  }

  const finish = () => {
    onClose()
    onCompleted?.()
  }

  return (
    <div className="fixed inset-0 z-40 flex items-center justify-center bg-black/60 backdrop-blur-sm p-4">
      <div className="bg-card border border-border/60 rounded-2xl sm:rounded-3xl shadow-2xl w-full max-w-xl relative overflow-hidden max-h-[90vh] overflow-y-auto">
        <button
          onClick={onClose}
          className="absolute right-3 sm:right-4 top-3 sm:top-4 p-1.5 rounded-full bg-background/80 hover:bg-background text-muted-foreground hover:text-foreground transition-colors z-10"
        >
          <X className="w-4 h-4" />
        </button>

        <div className="px-4 sm:px-6 pt-4 sm:pt-6 pb-3 sm:pb-4 border-b border-border/40 bg-gradient-to-r from-primary/10 via-background to-background">
          <div className="text-[10px] sm:text-xs font-semibold uppercase tracking-[0.2em] text-primary/70 mb-1">First-time setup</div>
          <h2 className="text-lg sm:text-xl font-bold flex items-center gap-2 pr-8">
            <span>Connect SentinelFS in 3 steps</span>
          </h2>
          <p className="text-[10px] sm:text-xs text-muted-foreground mt-1">
            Follow this wizard to quickly create a secure mesh between two devices on the same network.
          </p>

          <div className="flex items-center gap-2 mt-3 sm:mt-4 text-xs">
            {[1, 2, 3].map(i => (
              <div
                key={i}
                className={`flex-1 h-1 sm:h-1.5 rounded-full ${
                  i <= step ? 'bg-primary' : 'bg-border'
                }`}
              />
            ))}
          </div>
        </div>

        <div className="p-4 sm:p-6 space-y-4">
          {step === 1 && (
            <div className="space-y-4">
              <div className="flex items-center gap-2 sm:gap-3">
                <div className="p-2 sm:p-3 rounded-xl sm:rounded-2xl bg-primary/10 text-primary">
                  <KeyRound className="w-5 h-5" />
                </div>
                <div>
                  <h3 className="font-semibold">Step 1: Create a session code</h3>
                  <p className="text-xs text-muted-foreground">
                    All devices must use the same session code. You can generate a new one here or reuse an existing code.
                  </p>
                </div>
              </div>

              <div className="text-xs bg-muted/60 border border-border/60 rounded-xl p-3 leading-relaxed">
                <p className="font-semibold mb-1">Recommended:</p>
                <p>Generate a new session code on this device and then enter the same code on your other device.</p>
              </div>

              {error && (
                <div className="text-xs text-destructive bg-destructive/10 border border-destructive/40 rounded-lg px-3 py-2">
                  {error}
                </div>
              )}

              <div className="flex items-center justify-between mt-2">
                <button
                  onClick={handleGenerateCode}
                  disabled={busy}
                  className="inline-flex items-center gap-2 text-xs px-4 py-2 rounded-xl bg-primary text-primary-foreground hover:bg-primary/90 disabled:opacity-60 disabled:cursor-not-allowed"
                >
                  <ArrowRight className="w-3 h-3" />
                  <span>Generate new session code</span>
                </button>
                {sessionCode && (
                  <span className="text-xs status-success flex items-center gap-1">
                    <CheckCircle2 className="w-3 h-3" /> Code configured
                  </span>
                )}
              </div>
            </div>
          )}

          {step === 2 && (
            <div className="space-y-4">
              <div className="flex items-center gap-3">
                <div className="p-3 rounded-2xl bg-success-muted status-success">
                  <FolderPlus className="w-5 h-5" />
                </div>
                <div>
                  <h3 className="font-semibold">Step 2: Choose a folder to watch</h3>
                  <p className="text-xs text-muted-foreground">
                    SentinelFS will watch this folder for changes and sync them to your peers in near real time.
                  </p>
                </div>
              </div>

              {watchFolder ? (
                <div className="text-xs bg-muted/60 border border-border/60 rounded-xl p-3">
                  <div className="font-semibold mb-1">Selected folder:</div>
                  <div className="font-mono text-[11px] break-all">{watchFolder}</div>
                </div>
              ) : (
                <div className="text-xs bg-muted/60 border border-border/60 rounded-xl p-3 leading-relaxed">
                  Choose a folder on this device that you want to sync. You can add more folders later from the Settings &gt; Files section.
                </div>
              )}

              {error && (
                <div className="text-xs text-destructive bg-destructive/10 border border-destructive/40 rounded-lg px-3 py-2">
                  {error}
                </div>
              )}

              <button
                onClick={handleSelectFolder}
                disabled={busy}
                className="inline-flex items-center gap-2 text-xs px-4 py-2 rounded-xl btn-success disabled:opacity-60 disabled:cursor-not-allowed"
              >
                <FolderPlus className="w-3 h-3" />
                <span>Select folder</span>
              </button>
            </div>
          )}

          {step === 3 && (
            <div className="space-y-4">
              <div className="flex items-center gap-3">
                <div className="p-3 rounded-2xl bg-accent/10 text-accent-foreground">
                  <CheckCircle2 className="w-5 h-5" />
                </div>
                <div>
                  <h3 className="font-semibold">Step 3: Connect your other device</h3>
                  <p className="text-xs text-muted-foreground">
                    Now open SentinelFS on the other device on the same network and apply the same session code and folder settings.
                  </p>
                </div>
              </div>

              <div className="text-xs bg-muted/60 border border-border/60 rounded-xl p-3 leading-relaxed space-y-1">
                <p className="font-semibold mb-1">On the other device:</p>
                <ol className="list-decimal list-inside space-y-1">
                  <li>Open SentinelFS GUI.</li>
                  <li>Enter the same <span className="font-mono">session code</span> used on this device.</li>
                  <li>Add a similar folder as a watch folder (for example, Documents/SentinelFS).</li>
                </ol>
                <p className="mt-2">
                  As long as both devices are on the same LAN and discovery is enabled, peers should be discovered automatically.
                </p>
              </div>
            </div>
          )}
        </div>

        <div className="px-6 pb-4 pt-3 border-t border-border/40 flex items-center justify-between text-xs bg-background/80">
          <div className="text-muted-foreground">
            Step {step} / 3
          </div>
          <div className="flex items-center gap-2">
            {step > 1 && (
              <button
                onClick={back}
                className="px-3 py-1.5 rounded-lg border border-border/60 text-muted-foreground hover:bg-muted/60"
              >
                Back
              </button>
            )}
            {step < 3 && (
              <button
                onClick={next}
                className="px-3 py-1.5 rounded-lg bg-primary text-primary-foreground hover:bg-primary/90 flex items-center gap-1"
              >
                Next
                <ArrowRight className="w-3 h-3" />
              </button>
            )}
            {step === 3 && (
              <button
                onClick={finish}
                className="px-3 py-1.5 rounded-lg btn-success flex items-center gap-1"
              >
                Finish
                <CheckCircle2 className="w-3 h-3" />
              </button>
            )}
          </div>
        </div>
      </div>
    </div>
  )
}
