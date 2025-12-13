// Auto-generated from ErrorCodes.h - DO NOT EDIT DIRECTLY
export enum ErrorCode {
    CONNECTION_FAILED = 1000,
    PEER_NOT_FOUND = 1001,
    DISCOVERY_FAILED = 1002,
    BANDWIDTH_LIMIT_EXCEEDED = 1003,
    SESSION_CODE_MISMATCH = 2000,
    CERTIFICATE_VERIFICATION_FAILED = 2001,
    ENCRYPTION_FAILED = 2002,
    AUTHENTICATION_FAILED = 2003,
    FILE_NOT_FOUND = 3000,
    CONFLICT_DETECTED = 3001,
    SYNC_IN_PROGRESS = 3002,
    DELTA_GENERATION_FAILED = 3003,
    DISK_FULL = 4000,
    PERMISSION_DENIED = 4001,
    FILE_CORRUPTED = 4002,
    DAEMON_NOT_RUNNING = 5000,
    INTERNAL_ERROR = 5001,
    INVALID_CONFIGURATION = 5002,
    SUCCESS = 0,
}

export interface ErrorInfo {
    code: ErrorCode;
    message: string;
    details: string;
}

export class ErrorUtils {
    static getErrorMessage(code: ErrorCode): string {
        const messages: Record<ErrorCode, string> = {
            [ErrorCode.CONNECTION_FAILED]: 'Failed to establish connection',
            [ErrorCode.PEER_NOT_FOUND]: 'Peer not found in network',
            [ErrorCode.DISCOVERY_FAILED]: 'Peer discovery failed',
            [ErrorCode.BANDWIDTH_LIMIT_EXCEEDED]: 'Bandwidth limit exceeded',
            [ErrorCode.SESSION_CODE_MISMATCH]: 'Session code mismatch between peers',
            [ErrorCode.CERTIFICATE_VERIFICATION_FAILED]: 'Certificate verification failed',
            [ErrorCode.ENCRYPTION_FAILED]: 'Encryption operation failed',
            [ErrorCode.AUTHENTICATION_FAILED]: 'Authentication failed',
            [ErrorCode.FILE_NOT_FOUND]: 'File not found',
            [ErrorCode.CONFLICT_DETECTED]: 'File conflict detected',
            [ErrorCode.SYNC_IN_PROGRESS]: 'Synchronization already in progress',
            [ErrorCode.DELTA_GENERATION_FAILED]: 'Failed to generate file delta',
            [ErrorCode.DISK_FULL]: 'Disk space full',
            [ErrorCode.PERMISSION_DENIED]: 'Permission denied',
            [ErrorCode.FILE_CORRUPTED]: 'File corrupted',
            [ErrorCode.DAEMON_NOT_RUNNING]: 'SentinelFS daemon is not running',
            [ErrorCode.INTERNAL_ERROR]: 'Internal system error',
            [ErrorCode.INVALID_CONFIGURATION]: 'Invalid configuration',
            [ErrorCode.SUCCESS]: 'Operation successful',
        };
        return messages[code] || 'Unknown error';
    }
    
    static createErrorInfo(code: ErrorCode, details: string = ''): ErrorInfo {
        return {
            code,
            message: this.getErrorMessage(code),
            details
        };
    }
    
    static parseErrorJson(json: string): ErrorInfo {
        try {
            const parsed = JSON.parse(json);
            return {
                code: parsed.code,
                message: parsed.message,
                details: parsed.details || ''
            };
        } catch {
            return this.createErrorInfo(ErrorCode.INTERNAL_ERROR, 'Failed to parse error JSON');
        }
    }
}
