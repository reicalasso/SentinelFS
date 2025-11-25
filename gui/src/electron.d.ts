export interface ElectronAPI {
  on: (channel: string, callback: (data: any) => void) => void
  off: (channel: string, callback: (data: any) => void) => void
  sendCommand: (command: string) => Promise<{ success: boolean; error?: string }>
  selectFolder: () => Promise<string | null>
}

declare global {
  interface Window {
    api: ElectronAPI
  }
}
