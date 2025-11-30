#pragma once

/**
 * @file Constants.h
 * @brief Centralized configuration constants for SentinelFS
 * 
 * All magic numbers and configuration values should be defined here
 * to ensure consistency across the codebase.
 */

#include <cstddef>
#include <cstdint>

namespace sfs::config {

// =============================================================================
// Delta Sync Configuration
// =============================================================================

/// Block size for delta signature calculation (bytes)
constexpr std::size_t DELTA_BLOCK_SIZE = 4096;

/// Chunk size for network transfer (bytes)
constexpr std::size_t NETWORK_CHUNK_SIZE = 64 * 1024;  // 64KB

/// Threshold for streaming delta apply (bytes)
constexpr std::size_t LARGE_FILE_THRESHOLD = 100 * 1024 * 1024;  // 100MB

/// Number of blocks to process per thread pool task
constexpr std::size_t SIGNATURE_BATCH_SIZE = 16;

// =============================================================================
// Network Configuration
// =============================================================================

/// RTT measurement timeout (seconds)
constexpr int RTT_TIMEOUT_SEC = 2;

/// TCP server backlog size
constexpr int TCP_BACKLOG = 10;

/// Default TCP port for data transfer
constexpr int DEFAULT_TCP_PORT = 8080;

/// Default UDP port for peer discovery
constexpr int DEFAULT_DISCOVERY_PORT = 9999;

/// Default metrics server port
constexpr int DEFAULT_METRICS_PORT = 9100;

// =============================================================================
// Timeout Configuration
// =============================================================================

/// Pending chunk timeout before cleanup (seconds)
constexpr int PENDING_CHUNK_TIMEOUT_SEC = 300;  // 5 minutes

/// Cleanup thread interval (seconds)
constexpr int CLEANUP_INTERVAL_SEC = 60;  // 1 minute

/// Peer discovery broadcast interval (seconds)
constexpr int DISCOVERY_BROADCAST_INTERVAL_SEC = 5;

/// Peer status display interval (seconds)
constexpr int STATUS_DISPLAY_INTERVAL_SEC = 30;

/// RTT measurement interval (seconds)
constexpr int RTT_MEASUREMENT_INTERVAL_SEC = 15;

// =============================================================================
// Buffer Sizes
// =============================================================================

/// Maximum log file size (MB)
constexpr std::size_t MAX_LOG_FILE_SIZE_MB = 100;

/// Maximum logs to keep in GUI
constexpr std::size_t MAX_GUI_LOGS = 100;

/// IPC socket buffer size
constexpr std::size_t IPC_BUFFER_SIZE = 8192;

// =============================================================================
// Limits
// =============================================================================

/// Maximum concurrent peer connections
constexpr std::size_t MAX_PEER_CONNECTIONS = 100;

/// Maximum pending file transfers
constexpr std::size_t MAX_PENDING_TRANSFERS = 1000;

/// Ignore list entry timeout (seconds)
constexpr int IGNORE_LIST_TIMEOUT_SEC = 5;

} // namespace sfs::config
