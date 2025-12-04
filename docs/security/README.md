# SentinelFS Security Profiles

This directory contains Mandatory Access Control (MAC) profiles for hardening SentinelFS deployments.

## Overview

SentinelFS can be secured using Linux MAC systems:
- **AppArmor** - Default on Ubuntu, Debian, SUSE
- **SELinux** - Default on RHEL, Fedora, CentOS

These profiles restrict what the SentinelFS daemon can access, providing defense-in-depth security.

## AppArmor

### Installation

```bash
# Copy profile to AppArmor directory
sudo cp apparmor/sentinelfs.apparmor /etc/apparmor.d/usr.bin.sentinel_daemon

# Load the profile
sudo apparmor_parser -r /etc/apparmor.d/usr.bin.sentinel_daemon

# Verify profile is loaded
sudo aa-status | grep sentinel
```

### Testing (Complain Mode)

Before enforcing, test in complain mode to catch issues:

```bash
# Set to complain mode (logs violations but doesn't block)
sudo aa-complain /usr/bin/sentinel_daemon

# Run the daemon and monitor logs
sudo journalctl -f | grep -i apparmor

# After testing, enforce the profile
sudo aa-enforce /usr/bin/sentinel_daemon
```

### Customization

Edit the profile to match your deployment:

1. **Sync directories**: Add paths where users store synced files
2. **Socket location**: Adjust IPC socket path if non-default
3. **TLS certificates**: Update certificate paths if using custom locations

### Profiles Included

| Profile | Description |
|---------|-------------|
| `sentinel_daemon` | Main daemon (enforced) |
| `sentinel_cli` | CLI tool (enforced) |
| `sentinel_gui` | Electron GUI (complain mode) |

## SELinux

### Prerequisites

```bash
# Install SELinux policy development tools
# Fedora/RHEL:
sudo dnf install selinux-policy-devel policycoreutils-python-utils

# Ubuntu (if using SELinux):
sudo apt install selinux-policy-dev policycoreutils-python-utils
```

### Installation

```bash
cd selinux/

# Compile the policy module
checkmodule -M -m -o sentinelfs.mod sentinelfs.te
semodule_package -o sentinelfs.pp -m sentinelfs.mod -f sentinelfs.fc

# Install the module
sudo semodule -i sentinelfs.pp

# Apply file contexts
sudo restorecon -Rv /usr/bin/sentinel_daemon
sudo restorecon -Rv /etc/sentinelfs
sudo restorecon -Rv /var/lib/sentinelfs
sudo restorecon -Rv /var/log/sentinelfs
sudo restorecon -Rv /run/sentinelfs
```

### Troubleshooting

Check for denials:

```bash
# View recent AVC denials
sudo ausearch -m avc -ts recent

# Generate allow rules from denials
sudo ausearch -m avc -ts recent | audit2allow

# Create local policy module from denials
sudo ausearch -m avc -ts recent | audit2allow -M sentinelfs_local
sudo semodule -i sentinelfs_local.pp
```

### Permissive Mode

Test without blocking:

```bash
# Set SentinelFS domain to permissive
sudo semanage permissive -a sentinelfs_t

# After testing, remove permissive
sudo semanage permissive -d sentinelfs_t
```

## Security Considerations

### What These Profiles Protect Against

1. **Unauthorized file access**: Daemon can only access designated directories
2. **Network restrictions**: Limited to necessary ports and protocols
3. **Privilege escalation**: Prevents execution of arbitrary binaries
4. **Information disclosure**: Blocks access to sensitive system files

### Recommended Additional Hardening

1. **Run as non-root**: Create a dedicated `sentinelfs` user
2. **Systemd sandboxing**: Use systemd security options (see `sentinel_daemon.service`)
3. **Network segmentation**: Use firewall rules to limit peer connections
4. **TLS everywhere**: Enable TLS for all network connections
5. **Audit logging**: Enable `auditd` for comprehensive logging

### File Permissions Summary

| Path | Owner | Mode | Purpose |
|------|-------|------|---------|
| `/etc/sentinelfs/` | root:sentinelfs | 0750 | Configuration |
| `/etc/sentinelfs/certs/private/` | root:sentinelfs | 0700 | Private keys |
| `/var/lib/sentinelfs/` | sentinelfs:sentinelfs | 0750 | Data storage |
| `/var/log/sentinelfs/` | sentinelfs:sentinelfs | 0750 | Logs |
| `/run/sentinelfs/` | sentinelfs:sentinelfs | 0755 | Runtime |
| `/run/sentinelfs/daemon.sock` | sentinelfs:sentinelfs | 0660 | IPC socket |

## Verification

### Check AppArmor Status

```bash
sudo aa-status
# Look for sentinel_daemon in "profiles in enforce mode"
```

### Check SELinux Status

```bash
# Verify module is loaded
sudo semodule -l | grep sentinelfs

# Check file contexts
ls -lZ /usr/bin/sentinel_daemon
ls -lZ /var/lib/sentinelfs/
```

### Test Restrictions

```bash
# These should be blocked by the profiles:

# Attempt to read /etc/shadow (should fail)
sudo -u sentinelfs cat /etc/shadow

# Attempt to write outside allowed paths (should fail)
sudo -u sentinelfs touch /root/test
```

## Support

If you encounter issues with these security profiles:

1. Check logs: `journalctl -u sentinelfs` and `/var/log/audit/audit.log`
2. Run in complain/permissive mode to identify blocked operations
3. Report issues with denial messages to help improve the profiles
