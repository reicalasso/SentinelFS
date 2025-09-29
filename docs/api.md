# 🔧 SentinelFS API Reference

This document provides a comprehensive reference for the SentinelFS REST API. All endpoints are versioned under `/api/v1/`.

## 📋 API Overview

- **Base URL**: `http://<sentinelfs-host>:8080/api/v1`
- **Content-Type**: `application/json`
- **Authentication**: JWT tokens for most endpoints
- **Rate Limiting**: Requests are limited to 1000 per minute per IP

## 🔐 Authentication

Most endpoints require authentication using JWT tokens. To obtain a token:

```bash
curl -X POST http://localhost:8080/api/v1/auth/login \
    -H "Content-Type: application/json" \
    -d '{
        "username": "admin",
        "password": "your-password"
    }'
```

Include the returned token in the Authorization header for subsequent requests:

`Authorization: Bearer <jwt-token>`

## 📁 File Operations

### Get File Information

**GET** `/files/{path}`

Returns metadata and security information about a file.

#### Parameters
- `path` (path): File path, URL encoded

#### Request Example
```bash
curl -X GET "http://localhost:8080/api/v1/files/sentinels%2Fmydata.txt" \
    -H "Authorization: Bearer <jwt-token>"
```

#### Response
```json
{
    "path": "/sentinel/mydata.txt",
    "size": 1024,
    "created": "2023-10-01T12:00:00Z",
    "modified": "2023-10-01T12:00:00Z",
    "permissions": {
        "owner": "alice",
        "group": "users",
        "mode": "644"
    },
    "security": {
        "scanned": true,
        "threat_score": 0.0,
        "quarantined": false,
        "yara_matches": [],
        "entropy_score": 4.2
    }
}
```

### Upload File

**POST** `/files/{path}`

Uploads a file to the specified path.

#### Parameters
- `path` (path): Destination file path, URL encoded

#### Request Body
Raw file content or multipart form data

#### Request Example
```bash
curl -X POST "http://localhost:8080/api/v1/files/sentinels%2Fnewfile.txt" \
    -H "Authorization: Bearer <jwt-token>" \
    -H "Content-Type: application/octet-stream" \
    --data-binary @localfile.txt
```

#### Response
```json
{
    "path": "/sentinel/newfile.txt",
    "size": 1024,
    "uploaded": true,
    "security_status": "clean",
    "threat_score": 0.0,
    "scan_duration_ms": 120
}
```

### Download File

**GET** `/files/{path}/content`

Downloads the content of a file.

#### Parameters
- `path` (path): File path, URL encoded

#### Request Example
```bash
curl -X GET "http://localhost:8080/api/v1/files/sentinels%2Fmydata.txt/content" \
    -H "Authorization: Bearer <jwt-token>" \
    -o downloaded_file.txt
```

#### Response
File content with appropriate Content-Type header.

### Delete File

**DELETE** `/files/{path}`

Deletes a file at the specified path.

#### Parameters
- `path` (path): File path, URL encoded

#### Request Example
```bash
curl -X DELETE "http://localhost:8080/api/v1/files/sentinels%2Ftodelete.txt" \
    -H "Authorization: Bearer <jwt-token>"
```

#### Response
```json
{
    "deleted": true,
    "path": "/sentinel/todelete.txt"
}
```

## 🔐 Permission Management

### Get File Permissions

**GET** `/files/{path}/permissions`

Returns the current permissions for a file.

#### Parameters
- `path` (path): File path, URL encoded

#### Request Example
```bash
curl -X GET "http://localhost:8080/api/v1/files/sentinels%2Fmydata.txt/permissions" \
    -H "Authorization: Bearer <jwt-token>"
```

#### Response
```json
{
    "path": "/sentinel/mydata.txt",
    "permissions": [
        {
            "user": "alice",
            "type": "user",
            "permissions": ["read", "write"],
            "expiration": "2024-12-31T23:59:59Z"
        },
        {
            "user": "bob",
            "type": "user",
            "permissions": ["read"],
            "expiration": null
        }
    ]
}
```

### Update File Permissions

**PUT** `/files/{path}/permissions`

Updates permissions for a file.

#### Parameters
- `path` (path): File path, URL encoded

#### Request Body
```json
{
    "permissions": [
        {
            "user": "alice",
            "type": "user",
            "permissions": ["read", "write"],
            "expiration": "2024-12-31T23:59:59Z"
        }
    ]
}
```

#### Request Example
```bash
curl -X PUT "http://localhost:8080/api/v1/files/sentinels%2Fmydata.txt/permissions" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer <jwt-token>" \
    -d '{
        "permissions": [
            {
                "user": "bob",
                "type": "user",
                "permissions": ["read", "write", "execute"],
                "expiration": null
            }
        ]
    }'
```

#### Response
```json
{
    "updated": true,
    "path": "/sentinel/mydata.txt",
    "permissions": [
        {
            "user": "bob",
            "type": "user",
            "permissions": ["read", "write", "execute"],
            "expiration": null
        }
    ]
}
```

## 🛡️ Security Operations

### Scan File

**POST** `/security/scan/{path}`

Manually triggers a security scan for a file.

#### Parameters
- `path` (path): File path, URL encoded

#### Request Example
```bash
curl -X POST "http://localhost:8080/api/v1/security/scan/sentinels%2Fnewfile.txt" \
    -H "Authorization: Bearer <jwt-token>"
```

#### Response
```json
{
    "path": "/sentinel/newfile.txt",
    "scanned": true,
    "threat_score": 0.0,
    "scan_duration_ms": 120,
    "yara_results": [],
    "entropy_score": 4.2,
    "status": "clean"
}
```

### Get Quarantine List

**GET** `/security/quarantine`

Returns a list of files currently in quarantine.

#### Request Example
```bash
curl -X GET "http://localhost:8080/api/v1/security/quarantine" \
    -H "Authorization: Bearer <jwt-token>"
```

#### Response
```json
{
    "quarantined_files": [
        {
            "path": "/sentinel/.quarantine/suspicious.exe",
            "original_path": "/sentinel/uploads/suspicious.exe",
            "reason": "YARA_RULE_MATCH",
            "timestamp": "2023-10-01T12:00:00Z",
            "threat_score": 95.5
        }
    ],
    "total_count": 1
}
```

### Release from Quarantine

**POST** `/security/quarantine/release`

Releases a file from quarantine.

#### Request Body
```json
{
    "file_path": "/sentinel/.quarantine/suspicious.exe",
    "action": "release_and_restore"
}
```

#### Request Example
```bash
curl -X POST "http://localhost:8080/api/v1/security/quarantine/release" \
    -H "Content-Type: application/json" \
    -H "Authorization: Bearer <jwt-token>" \
    -d '{
        "file_path": "/sentinel/.quarantine/suspicious.exe",
        "action": "release_and_restore"
    }'
```

#### Response
```json
{
    "released": true,
    "original_path": "/sentinel/uploads/suspicious.exe",
    "action": "release_and_restore"
}
```

## 🧠 AI Operations

### Get AI Analysis

**GET** `/ai/analysis/{path}`

Returns AI-based behavioral analysis for a file.

#### Parameters
- `path` (path): File path, URL encoded

#### Request Example
```bash
curl -X GET "http://localhost:8080/api/v1/ai/analysis/sentinels%2Fmydata.txt" \
    -H "Authorization: Bearer <jwt-token>"
```

#### Response
```json
{
    "path": "/sentinel/mydata.txt",
    "analysis": {
        "access_pattern_score": 0.2,
        "behavior_normal": true,
        "anomaly_detected": false,
        "confidence": 0.92,
        "last_updated": "2023-10-01T12:00:00Z"
    }
}
```

## 📊 Monitoring & Health

### Health Check

**GET** `/health`

Returns the health status of the SentinelFS node.

#### Request Example
```bash
curl -X GET "http://localhost:8080/api/v1/health"
```

#### Response
```json
{
    "status": "healthy",
    "timestamp": "2023-10-01T12:00:00Z",
    "version": "1.0.0",
    "uptime_seconds": 3600,
    "nodes": 3,
    "storage": {
        "total": 107374182400,
        "used": 21474836480,
        "available": 85899345920
    }
}
```

### Get Metrics

**GET** `/metrics`

Returns Prometheus-formatted metrics.

#### Request Example
```bash
curl -X GET "http://localhost:8080/api/v1/metrics"
```

#### Response
Prometheus metrics in text format.

### Get Audit Logs

**GET** `/audit`

Returns audit logs based on query parameters.

#### Query Parameters
- `limit` (optional): Number of entries to return (default: 50, max: 1000)
- `offset` (optional): Offset for pagination
- `time_range` (optional): Time range to query (e.g., "24h", "7d")
- `user` (optional): Filter by user
- `action` (optional): Filter by action type
- `severity` (optional): Filter by severity level

#### Request Example
```bash
curl -X GET "http://localhost:8080/api/v1/audit?limit=100&time_range=24h&severity=critical" \
    -H "Authorization: Bearer <jwt-token>"
```

#### Response
```json
{
    "logs": [
        {
            "id": "log-123",
            "timestamp": "2023-10-01T12:00:00Z",
            "user": "alice",
            "action": "file_write",
            "resource": "/sentinel/important.txt",
            "status": "success",
            "severity": "critical",
            "details": {
                "file_size": 1024,
                "threat_score": 0.0
            }
        }
    ],
    "total_count": 1,
    "has_more": false
}
```

## 📝 Examples

### Using curl

```bash
# Get file information
curl -X GET "http://localhost:8080/api/v1/files/sentinels%2Fmydata.txt" \
    -H "Authorization: Bearer $JWT_TOKEN" \
    -H "Content-Type: application/json"
```

### Using Python requests

```python
import requests
import json

BASE_URL = "http://localhost:8080/api/v1"
headers = {
    "Authorization": f"Bearer {jwt_token}",
    "Content-Type": "application/json"
}

# Get file information
response = requests.get(f"{BASE_URL}/files/sentinels/mydata.txt", headers=headers)
data = response.json()
print(f"File size: {data['size']} bytes")

# Upload a file
with open('local_file.txt', 'rb') as f:
    response = requests.post(
        f"{BASE_URL}/files/sentinels/uploaded.txt",
        headers=headers,
        data=f
    )
    result = response.json()
    print(f"Upload status: {result['security_status']}")
```

### Using Go

```go
package main

import (
    "bytes"
    "encoding/json"
    "fmt"
    "io"
    "net/http"
)

const baseURL = "http://localhost:8080/api/v1"

func getFileInfo(path, token string) (map[string]interface{}, error) {
    client := &http.Client{}
    req, err := http.NewRequest("GET", fmt.Sprintf("%s/files/%s", baseURL, path), nil)
    if err != nil {
        return nil, err
    }
    
    req.Header.Set("Authorization", "Bearer "+token)
    req.Header.Set("Content-Type", "application/json")
    
    resp, err := client.Do(req)
    if err != nil {
        return nil, err
    }
    defer resp.Body.Close()
    
    body, err := io.ReadAll(resp.Body)
    if err != nil {
        return nil, err
    }
    
    var result map[string]interface{}
    err = json.Unmarshal(body, &result)
    if err != nil {
        return nil, err
    }
    
    return result, nil
}

func main() {
    token := "your-jwt-token"
    info, err := getFileInfo("sentinels/mydata.txt", token)
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        return
    }
    fmt.Printf("File size: %.0f bytes\n", info["size"])
}
```

_last updated 29.09.2025_