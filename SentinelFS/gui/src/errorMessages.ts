import { ErrorCode, ErrorInfo, ErrorUtils } from './errorCodes';

export type DaemonErrorCode = string

export interface FriendlyError {
  title: string
  message: string
  hint?: string
}

// Map type-safe error codes to human-friendly messages
export function mapDaemonError(errorInfo?: ErrorInfo | string): FriendlyError {
  let code: ErrorCode;
  let details: string = '';
  
  if (typeof errorInfo === 'string') {
    // Legacy fallback for string-based errors
    const parsed = ErrorUtils.parseErrorJson(errorInfo);
    code = parsed.code;
    details = parsed.details;
  } else if (errorInfo) {
    // New type-safe error handling
    code = errorInfo.code;
    details = errorInfo.details;
  } else {
    return {
      title: 'Unknown error',
      message: 'An unexpected error occurred.',
      hint: 'Please try again. If it keeps happening, share logs via a support bundle.'
    }
  }

  const baseMessage = ErrorUtils.getErrorMessage(code);
  
  switch (code) {
    case ErrorCode.CONNECTION_FAILED:
      return {
        title: 'Daemon connection failed',
        message: baseMessage,
        hint: 'Make sure the daemon is running, then restart the app and check your firewall/antivirus rules.'
      }

    case ErrorCode.SESSION_CODE_MISMATCH:
      return {
        title: 'Session code mismatch',
        message: baseMessage,
        hint: 'Ensure all peers are configured with the exact same session code.'
      }

    case ErrorCode.DISCOVERY_FAILED:
      return {
        title: 'Peer discovery failed',
        message: baseMessage,
        hint: 'Verify that devices are on the same LAN and that UDP broadcast is not blocked by a firewall.'
      }

    case ErrorCode.PERMISSION_DENIED:
      return {
        title: 'Permission error',
        message: baseMessage,
        hint: 'Make sure the current user has access to the folders and network resources you are using.'
      }

    case ErrorCode.DAEMON_NOT_RUNNING:
      return {
        title: 'Daemon not running',
        message: baseMessage,
        hint: 'Start the SentinelFS daemon service and restart the GUI application.'
      }

    default:
      return {
        title: 'Operation failed',
        message: baseMessage,
        hint: details || 'Check the Debug Console tab for more details.'
      }
  }
}
