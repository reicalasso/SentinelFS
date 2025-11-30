import { ipcRenderer, contextBridge } from 'electron'

// --------- Expose some API to the Renderer process ---------
const validChannels = ['daemon-status', 'daemon-data', 'daemon-log']
const listenerRegistry = new Map<string, Map<(...args: any[]) => void, (...args: any[]) => void>>()

contextBridge.exposeInMainWorld('api', {
  on: (channel: string, callback: (data: any) => void) => {
    if (!validChannels.includes(channel)) return

    const wrapped = (_event: any, ...args: any[]) => callback(args[0])
    ipcRenderer.on(channel, wrapped)

    if (!listenerRegistry.has(channel)) {
      listenerRegistry.set(channel, new Map())
    }
    listenerRegistry.get(channel)!.set(callback, wrapped)
  },
  off: (channel: string, callback: (data: any) => void) => {
    const map = listenerRegistry.get(channel)
    if (!map) return

    const wrapped = map.get(callback)
    if (wrapped) {
      ipcRenderer.removeListener(channel, wrapped)
      map.delete(callback)
    }
  },
  sendCommand: (command: string) => ipcRenderer.invoke('send-command', command),
  selectFolder: () => ipcRenderer.invoke('select-folder'),
})
