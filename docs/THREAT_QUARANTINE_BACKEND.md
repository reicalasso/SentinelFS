# SentinelFS Threat Quarantine Backend

## Overview
Backend implementation guide for the threat quarantine feature.

## Required Commands

### DELETE_THREAT
Permanently deletes a quarantined threat.

**Format:** `DELETE_THREAT <threat_id>`

**Response:**
- Success: `{ success: true }`
- Error: `{ success: false, error: "message" }`

### MARK_THREAT_SAFE
Marks a threat as safe (keeps in quarantine).

**Format:** `MARK_THREAT_SAFE <threat_id>`

### UNMARK_THREAT_SAFE
Removes safe mark from threat.

**Format:** `UNMARK_THREAT_SAFE <threat_id>`

---

## Data Transfer

### DETECTED_THREATS Event
Send detected threats to GUI:

```json
{
  "type": "DETECTED_THREATS",
  "payload": [
    {
      "id": 1,
      "filePath": "/path/to/file",
      "threatType": "RANSOMWARE",
      "threatLevel": "CRITICAL",
      "threatScore": 95.5,
      "detectedAt": 1701234567890,
      "markedSafe": false
    }
  ]
}
```

### Threat Types
- `RANSOMWARE`
- `HIGH_ENTROPY`
- `MASS_OPERATION`
- `SUSPICIOUS_PATTERN`
- `UNKNOWN`

---

## Database Schema

```sql
CREATE TABLE detected_threats (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  file_path TEXT NOT NULL,
  threat_type TEXT NOT NULL,
  threat_level TEXT NOT NULL,
  threat_score REAL NOT NULL,
  detected_at INTEGER NOT NULL,
  marked_safe INTEGER DEFAULT 0
);
```

---

## Implementation Checklist

### Backend (C++)
- [ ] DELETE_THREAT command
- [ ] MARK_THREAT_SAFE command
- [ ] UNMARK_THREAT_SAFE command
- [ ] Quarantine directory management
- [ ] DETECTED_THREATS event
- [ ] Database CRUD operations

### ML Plugin
- [ ] Save threat detection results
- [ ] Move files to quarantine
- [ ] Notify daemon of threats

---

## Test Scenarios

1. **Threat Detection → List**
   - ML detects threat
   - GUI shows badge
   - User opens quarantine
   - All threats listed

2. **Delete Threat**
   - User selects threat
   - Clicks delete
   - Backend removes file
   - GUI updates list

3. **Mark Safe**
   - User marks threat safe
   - Backend updates DB
   - GUI shows green badge

---

## Notes

- All timestamps: Unix epoch (milliseconds)
- Hash format: SHA-256
- Quarantine dir: `~/.sentinelfs/quarantine/`
- Safe marked files stay in quarantine

---

*SentinelFS Backend Team - December 2025*
