import { ipcRenderer, contextBridge } from 'electron'

// --------- Expose some API to the Renderer process ---------
contextBridge.exposeInMainWorld('api', {
  on: (channel: string, callback: (data: any) => void) => {
    // Whitelist channels
    const validChannels = ['daemon-status', 'daemon-data', 'daemon-log']
    if (validChannels.includes(channel)) {
      // Deliberately strip event as it includes `sender` 
      ipcRenderer.on(channel, (_event, ...args) => callback(args[0]))
    }
  },
  off: (channel: string, callback: (data: any) => void) => {
    ipcRenderer.removeListener(channel, callback)
  },
  sendCommand: (command: string) => ipcRenderer.invoke('send-command', command),
})
