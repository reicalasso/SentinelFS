#!/usr/bin/env node

/**
 * Script to generate TypeScript error codes from C++ ErrorCodes.h
 * This ensures type-safe error handling between GUI and Core
 */

import * as fs from 'fs';
import * as path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

interface ErrorCode {
    name: string;
    value: number;
    comment: string;
}

function parseErrorCodes(headerPath: string): ErrorCode[] {
    const content = fs.readFileSync(headerPath, 'utf8');
    const lines = content.split('\n');
    const errorCodes: ErrorCode[] = [];
    
    let inEnum = false;
    let currentValue = 1000; // Starting value based on the actual enum
    
    for (const line of lines) {
        const trimmed = line.trim();
        
        if (trimmed.includes('enum class ErrorCode')) {
            inEnum = true;
            continue;
        }
        
        if (!inEnum) continue;
        
        if (trimmed === '};') {
            break;
        }
        
        // Skip comments and empty lines
        if (trimmed.startsWith('//') || trimmed === '' || trimmed.includes('Network Errors')) {
            continue;
        }
        
        // Parse enum values with explicit assignment
        const match = trimmed.match(/(\w+)\s*=\s*(\d+),?\s*(?:\/\/\s*(.*))?/);
        if (match) {
            errorCodes.push({
                name: match[1],
                value: parseInt(match[2]),
                comment: match[3] || ''
            });
            currentValue = parseInt(match[2]) + 1;
        } else {
            // Handle auto-increment values (though our enum doesn't use them)
            const autoMatch = trimmed.match(/(\w+),?\s*(?:\/\/\s*(.*))?/);
            if (autoMatch) {
                errorCodes.push({
                    name: autoMatch[1],
                    value: currentValue++,
                    comment: autoMatch[2] || ''
                });
            }
        }
    }
    
    return errorCodes;
}

function generateTypeScriptFile(errorCodes: ErrorCode[]): string {
    const imports = `// Auto-generated from ErrorCodes.h - DO NOT EDIT DIRECTLY
export enum ErrorCode {
`;

    const enumEntries = errorCodes.map(code => 
        `    ${code.name} = ${code.value},`
    ).join('\n');

    const enumEnd = `
}

export interface ErrorInfo {
    code: ErrorCode;
    message: string;
    details: string;
}

export class ErrorUtils {
    static getErrorMessage(code: ErrorCode): string {
        const messages: Record<ErrorCode, string> = {
`;

    // Create a map of error codes to their actual messages from C++
    const errorMessages: Record<string, string> = {
        'CONNECTION_FAILED': 'Failed to establish connection',
        'PEER_NOT_FOUND': 'Peer not found in network',
        'DISCOVERY_FAILED': 'Peer discovery failed',
        'BANDWIDTH_LIMIT_EXCEEDED': 'Bandwidth limit exceeded',
        'SESSION_CODE_MISMATCH': 'Session code mismatch between peers',
        'CERTIFICATE_VERIFICATION_FAILED': 'Certificate verification failed',
        'ENCRYPTION_FAILED': 'Encryption operation failed',
        'AUTHENTICATION_FAILED': 'Authentication failed',
        'FILE_NOT_FOUND': 'File not found',
        'CONFLICT_DETECTED': 'File conflict detected',
        'SYNC_IN_PROGRESS': 'Synchronization already in progress',
        'DELTA_GENERATION_FAILED': 'Failed to generate file delta',
        'DISK_FULL': 'Disk space full',
        'PERMISSION_DENIED': 'Permission denied',
        'FILE_CORRUPTED': 'File corrupted',
        'DAEMON_NOT_RUNNING': 'SentinelFS daemon is not running',
        'INTERNAL_ERROR': 'Internal system error',
        'INVALID_CONFIGURATION': 'Invalid configuration',
        'SUCCESS': 'Operation successful'
    };

    const messageEntries = errorCodes.map(code => 
            `            [ErrorCode.${code.name}]: '${errorMessages[code.name] || 'Unknown error'}',`
    ).join('\n');

    const classEnd = `
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
`;

    return imports + enumEntries + enumEnd + messageEntries + classEnd;
}

function main() {
    const headerPath = path.join(__dirname, '../core/include/ErrorCodes.h');
    const outputPath = path.join(__dirname, '../gui/src/errorCodes.ts');
    
    if (!fs.existsSync(headerPath)) {
        console.error(`Error: ${headerPath} not found`);
        process.exit(1);
    }
    
    const errorCodes = parseErrorCodes(headerPath);
    const tsContent = generateTypeScriptFile(errorCodes);
    
    fs.writeFileSync(outputPath, tsContent);
    console.log(`Generated TypeScript error codes at ${outputPath}`);
    console.log(`Processed ${errorCodes.length} error codes`);
}

if (import.meta.url === `file://${process.argv[1]}`) {
    main();
}
