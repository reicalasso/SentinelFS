import { app, BrowserWindow, ipcMain, dialog } from 'electron'
import path from 'node:path'
import net from 'node:net'
import os from 'node:os'
import { spawn, ChildProcess } from 'node:child_process'

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
let daemonProcess: ChildProcess | null = null
const RECONNECT_INTERVAL = 5000

// Dynamic socket path based on XDG or fallback
// Must match PathUtils::getSocketPath() in C++ daemon
function getSocketPath() {
  const runtimeDir = process.env.XDG_RUNTIME_DIR
  if (runtimeDir) {
    return path.join(runtimeDir, 'sentinelfs', 'sentinel_daemon.sock')
  }
  return path.join(os.tmpdir(), 'sentinelfs', 'sentinel_daemon.sock')
}

function getDaemonPath() {
    if (app.isPackaged) {
        // In AppImage: resources/bin/sentinel_daemon
        return path.join(process.resourcesPath, 'bin', 'sentinel_daemon')
    } else {
        // Dev: SentinelFS/build/app/daemon/sentinel_daemon
        // __dirname is dist-electron, so ../.. points to SentinelFS root
        return path.join(__dirname, '../../build/app/daemon/sentinel_daemon')
    }
}

function startDaemon() {
    const daemonPath = getDaemonPath()
    console.log('Looking for daemon at:', daemonPath)
    
    // Check if daemon is already running by trying to connect
    const socket = net.createConnection(getSocketPath())
    socket.on('connect', () => {
        console.log('Daemon already running')
        socket.end()
        connectToDaemon()
    })
    socket.on('error', () => {
        // Not running, spawn it
        console.log('Spawning daemon...')
        
        // Determine plugin dir
        let pluginDir = ''
        if (app.isPackaged) {
            pluginDir = path.join(process.resourcesPath, 'lib', 'plugins')
        } else {
            pluginDir = path.join(__dirname, '../../build/plugins')
        }

        const projectRoot = app.isPackaged ? process.resourcesPath : path.resolve(__dirname, '../../')

        try {
            // Pass arguments if needed, e.g., config path
            daemonProcess = spawn(daemonPath, [], {
                stdio: 'ignore', // Daemon logs to file anyway, or use 'pipe' to capture
                detached: false,
                cwd: projectRoot,
                env: { ...process.env, SENTINELFS_PLUGIN_DIR: pluginDir }
            })
            
            daemonProcess.on('error', (err) => {
                console.error('Failed to spawn daemon:', err)
            })
            
            daemonProcess.on('exit', (code) => {
                console.log('Daemon exited with code:', code)
                daemonProcess = null
            })
            
            // Wait a bit for daemon to initialize socket
            setTimeout(connectToDaemon, 1000)
            
        } catch (e) {
            console.error('Error spawning daemon:', e)
        }
    })
}

const SOCKET_PATH = getSocketPath()

const distPath = app.isPackaged
  ? path.join(process.resourcesPath, 'app.asar', 'dist', 'index.html')
  : path.join(process.env.DIST as string, 'index.html')

function createWindow() {
  win = new BrowserWindow({
    width: 900,
    height: 670,
    minWidth: 800,
    minHeight: 600,
    title: 'SentinelFS Dashboard',
    icon: path.join(process.env.VITE_PUBLIC as string, 'sentinel-icon.svg'),
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
    win.loadFile(distPath)
  }

  // Connect to daemon when window is ready
  win.once('ready-to-show', () => {
    win?.show()
    // Instead of direct connect, try to start daemon first
    startDaemon()
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

  let buffer = ''
  
  daemonSocket.on('data', (data) => {
    buffer += data.toString()
    
    // Process complete lines
    let newlineIndex
    while ((newlineIndex = buffer.indexOf('\n')) !== -1) {
        const line = buffer.substring(0, newlineIndex).trim()
        buffer = buffer.substring(newlineIndex + 1)
        
        if (!line) continue
        
        try {
            // Try to parse as JSON
            if (line.startsWith('{') || line.startsWith('[')) {
                const json = JSON.parse(line)
                
                // Determine message type based on content
                if (json.syncStatus !== undefined) {
                    win?.webContents.send('daemon-data', { type: 'STATUS', payload: json })
                } else if (json.totalUploaded !== undefined) {
                    win?.webContents.send('daemon-data', { type: 'METRICS', payload: json })
                } else if (json.files !== undefined) {
                    win?.webContents.send('daemon-data', { type: 'FILES', payload: json.files })
                } else if (json.peers !== undefined) {
                    win?.webContents.send('daemon-data', { type: 'PEERS', payload: json.peers })
                } else if (json.activity !== undefined) {
                    win?.webContents.send('daemon-data', { type: 'ACTIVITY', payload: json.activity })
                } else if (json.transfers !== undefined) {
                    win?.webContents.send('daemon-data', { type: 'TRANSFERS', payload: json.transfers })
                } else if (json.tcpPort !== undefined || json.uploadLimit !== undefined) {
                    win?.webContents.send('daemon-data', { type: 'CONFIG', payload: json })
                } else if (Array.isArray(json)) {
                    // Fallback/Legacy: Distinguish between PEERS and FILES by checking first element
                    if (json.length > 0 && json[0].path !== undefined) {
                        win?.webContents.send('daemon-data', { type: 'FILES', payload: json })
                    } else {
                        win?.webContents.send('daemon-data', { type: 'PEERS', payload: json })
                    }
                } else {
                    win?.webContents.send('daemon-data', { type: 'UNKNOWN', payload: json })
                }
            } else {
                // Plain text log
                win?.webContents.send('daemon-log', line)
            }
        } catch (e) {
            // Not JSON, treat as log
            win?.webContents.send('daemon-log', line)
        }
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
            // Send multiple commands in sequence with proper spacing
            daemonSocket.write('STATUS_JSON\n')
            setTimeout(() => daemonSocket?.write('METRICS_JSON\n'), 150)
            setTimeout(() => daemonSocket?.write('PEERS_JSON\n'), 300)
            setTimeout(() => daemonSocket?.write('FILES_JSON\n'), 450)
            setTimeout(() => daemonSocket?.write('ACTIVITY_JSON\n'), 600)
            setTimeout(() => daemonSocket?.write('TRANSFERS_JSON\n'), 750)
            setTimeout(() => daemonSocket?.write('CONFIG_JSON\n'), 900)
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

ipcMain.handle('select-folder', async () => {
    const result = await dialog.showOpenDialog(win!, {
        properties: ['openDirectory']
    })
    
    if (result.canceled) {
        return null
    }
    
    return result.filePaths[0]
})
