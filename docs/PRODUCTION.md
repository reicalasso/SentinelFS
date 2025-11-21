# SentinelFS Production Checklist

## Cleanup & Hygiene
- TODO files archived under `docs/archive/` for reference.
- Placeholder test removed; `tests/placeholder_test.cpp` deleted.
- Unnecessary `.db` and log files are now located under XDG-friendly directories via `PathUtils`.

## Versioning & Metadata
- `CMakeLists.txt` defines `project(SentinelFS VERSION 1.0.0 LANGUAGES CXX)`.
- `core/include/Version.h` is generated from `Version.h.in`, exposing `Version::STRING` and numeric constants for downstream modules.

## Configuration & Paths
- Added `PathUtils` (core/include/PathUtils.h + core/src/PathUtils.cpp) to centralize: `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_RUNTIME_DIR`, socket path, and database path.
- `SentinelFS::Config` now boots from `~/.config/sentinelfs/sentinel.conf` (generated template when missing). CLI overrides still work.
- SQLite backend respects `SENTINEL_DB_PATH` (set to `${data_dir}/sentinel.db` in `sentinel_daemon.cpp`).

## IPC & Daemon
- IPC socket now lives under the runtime directory (not `/tmp`).
- All created directories are auto-generated with proper error handling.

## Installation & Systemd
- `CMake` build now installs binaries, libraries, and headers (`bin/`, `lib/`, `include/`).
- Added `docs/sentinel_daemon.service` as a template for downstream packaging.

## Next Steps
1. **System Integration**: Provide packaging scripts (`make install`, `deb/rpm` spec) and copy the `sentinel_daemon.service` into `/lib/systemd/system` during install. Document service parameters (`User`, `RuntimeDirectory`).
2. **Security Hardening**: Add TLS cert validation to the handshake, restrict IPC socket permissions, and consider AppArmor/SELinux profiles as part of deployment.
3. **Monitoring & Metrics**: Wire `MetricsCollector` output into a Prometheus exporter and expose health checks for `sentinel_daemon`.
4. **CI/CD**: Add GitHub Actions workflow to run `cmake --build`, linting, and the new bandwidth limiter test before merge.

Bookmark this file for future productization steps.
