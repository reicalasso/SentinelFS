export type DaemonErrorCode = string

export interface FriendlyError {
  title: string
  message: string
  hint?: string
}

// Map low-level daemon errors to human-friendly messages
export function mapDaemonError(raw?: string): FriendlyError {
  if (!raw) {
    return {
      title: 'Unknown error',
      message: 'An unexpected error occurred.',
      hint: 'Please try again. If it keeps happening, share logs via a support bundle.'
    }
  }

  const text = raw.toLowerCase()

  if (text.includes('connection refused') || text.includes('cannot connect')) {
    return {
      title: 'Daemon connection failed',
      message: 'The GUI could not connect to the SentinelFS daemon.',
      hint: 'Make sure the daemon is running, then restart the app and check your firewall/antivirus rules.'
    }
  }

  if (text.includes('session') && text.includes('code')) {
    return {
      title: 'Session code mismatch',
      message: 'The session code on this device does not match the remote peer.',
      hint: 'Ensure all peers are configured with the exact same session code.'
    }
  }

  if (text.includes('discovery') || text.includes('broadcast')) {
    return {
      title: 'Peer discovery failed',
      message: 'No peers were discovered on the network.',
      hint: 'Verify that devices are on the same LAN and that UDP broadcast is not blocked by a firewall.'
    }
  }

  if (text.includes('permission') || text.includes('access denied')) {
    return {
      title: 'Permission error',
      message: 'The requested operation does not have the required filesystem or network permissions.',
      hint: 'Make sure the current user has access to the folders and network resources you are using.'
    }
  }

  return {
    title: 'Operation failed',
    message: raw,
    hint: 'Check the Debug Console tab for more details.'
  }
}
