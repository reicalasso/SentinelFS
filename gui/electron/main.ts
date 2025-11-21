import { app, BrowserWindow, ipcMain } from 'electron'
import path from 'node:path'
import net from 'node:net'
import os from 'node:os'

// The built directory structure
//
// ├─┬─ dist
// │ └── index.html
// ├── dist-electron
// │ ├── main.js
// │ └── preload.js
//
process.env.DIST = path.join(__dirname, '../dist')
process.env.VITE_PUBLIC = app.isPackaged ? process.env.DIST : path.join(__dirname, '../public')

let win: BrowserWindow | null
let daemonSocket: net.Socket | null = null
const RECONNECT_INTERVAL = 5000

// Dynamic socket path based on XDG or fallback
function getSocketPath() {
  const runtimeDir = process.env.XDG_RUNTIME_DIR
  if (runtimeDir) {
    return path.join(runtimeDir, 'sentinelfs.sock')
  }
  return path.join(os.tmpdir(), 'sentinelfs.sock')
}

const SOCKET_PATH = getSocketPath()

function createWindow() {
  win = new BrowserWindow({
    width: 900,
    height: 670,
    minWidth: 800,
    minHeight: 600,
    title: 'SentinelFS Dashboard',
    icon: path.join(process.env.VITE_PUBLIC, 'sentinel-icon.svg'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      // Security: Sandbox enabled by default in recent Electron versions
    },
  })

  // Hide menu bar for cleaner look
  win.setMenuBarVisibility(false)

  if (process.env.VITE_DEV_SERVER_URL) {
    win.loadURL(process.env.VITE_DEV_SERVER_URL)
    // win.webContents.openDevTools()
  } else {
    win.loadFile(path.join(process.env.DIST, 'index.html'))
  }

  // Connect to daemon when window is ready
  win.once('ready-to-show', () => {
    win?.show()
    connectToDaemon()
  })
}

function connectToDaemon() {
  if (daemonSocket) {
    daemonSocket.destroy()
  }

  console.log(`Connecting to daemon at ${SOCKET_PATH}...`)
  daemonSocket = net.createConnection(SOCKET_PATH)

  daemonSocket.on('connect', () => {
    console.log('Connected to SentinelFS daemon')
    win?.webContents.send('daemon-status', 'connected')
    
    // Start monitoring loop
    startMonitoring()
  })

  daemonSocket.on('data', (data) => {
    const message = data.toString()
    // Here we would parse the JSON response from daemon
    // For now, just forward raw message
    console.log('Received from daemon:', message)
    try {
        // If it looks like JSON, parse it
        if (message.trim().startsWith('{')) {
            const json = JSON.parse(message)
            win?.webContents.send('daemon-data', json)
        } else {
             // Handle raw text responses (legacy)
             win?.webContents.send('daemon-log', message)
        }
    } catch (e) {
        console.error('Failed to parse daemon message:', e)
    }
  })

  daemonSocket.on('error', (err) => {
    console.error('Daemon connection error:', err.message)
    win?.webContents.send('daemon-status', 'error')
  })

  daemonSocket.on('close', () => {
    console.log('Daemon connection closed')
    win?.webContents.send('daemon-status', 'disconnected')
    daemonSocket = null
    
    // Try to reconnect
    setTimeout(connectToDaemon, RECONNECT_INTERVAL)
  })
}

let monitorInterval: NodeJS.Timeout | null = null

function startMonitoring() {
    if (monitorInterval) clearInterval(monitorInterval)
    
    // Poll daemon for status every 2 seconds
    monitorInterval = setInterval(() => {
        if (daemonSocket && !daemonSocket.destroyed) {
            // Send STATUS command
            // Protocol: "STATUS\n"
            daemonSocket.write('STATUS\n')
        }
    }, 2000)
}

// Quit when all windows are closed, except on macOS.
app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit()
    if (daemonSocket) daemonSocket.end()
  }
})

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow()
  }
})

app.whenReady().then(createWindow)

// IPC Handlers for Frontend -> Daemon communication
ipcMain.handle('send-command', async (_event, command: string) => {
    if (!daemonSocket || daemonSocket.destroyed) {
        return { success: false, error: 'Not connected to daemon' }
    }
    
    try {
        daemonSocket.write(command + '\n')
        return { success: true }
    } catch (err) {
        return { success: false, error: (err as Error).message }
    }
})
