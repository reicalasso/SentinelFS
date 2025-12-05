#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <map>

namespace SentinelFS {

/**
 * @brief Conflict resolution strategies
 */
enum class ResolutionStrategy {
    NEWEST_WINS,        // Keep the newest version based on timestamp
    MANUAL,             // Mark for manual user resolution
    KEEP_BOTH,          // Create conflict copies (.conflict.1, .conflict.2)
    LARGEST_WINS,       // Keep the version with more content
    REMOTE_WINS,        // Always prefer remote version
    LOCAL_WINS          // Always prefer local version
};

/**
 * @brief File conflict metadata
 */
struct FileConflict {
    std::string path;                           // File path
    std::string localHash;                      // SHA-256 of local version
    std::string remoteHash;                     // SHA-256 of remote version
    std::string remotePeerId;                   // Peer ID that sent conflicting version
    uint64_t localTimestamp;                    // Unix timestamp (milliseconds)
    uint64_t remoteTimestamp;                   // Unix timestamp (milliseconds)
    size_t localSize;                           // File size in bytes
    size_t remoteSize;                          // File size in bytes
    ResolutionStrategy strategy;                // How to resolve
    bool resolved;                              // Resolution status
    std::chrono::system_clock::time_point detectedAt;  // When conflict was detected
    
    FileConflict() 
        : localTimestamp(0), remoteTimestamp(0), 
          localSize(0), remoteSize(0),
          strategy(ResolutionStrategy::MANUAL), 
          resolved(false),
          detectedAt(std::chrono::system_clock::now()) {}
};

/**
 * @brief Vector clock for causality tracking (Lamport timestamps)
 * 
 * Simple implementation: each peer maintains a counter
 * Counter increments on every local modification
 */
struct VectorClock {
    std::map<std::string, uint64_t> clocks;  // peerId -> counter
    
    // Get clock value for a peer
    uint64_t get(const std::string& peerId) const {
        auto it = clocks.find(peerId);
        return (it != clocks.end()) ? it->second : 0;
    }
    
    // Increment local peer's clock
    void increment(const std::string& peerId) {
        clocks[peerId]++;
    }
    
    // Merge with another vector clock (take max of each peer)
    void merge(const VectorClock& other) {
        for (const auto& [peerId, counter] : other.clocks) {
            clocks[peerId] = std::max(clocks[peerId], counter);
        }
    }
    
    // Check if this clock happened-before another
    bool happensBefore(const VectorClock& other) const {
        bool anyLess = false;
        
        // Check all keys in this->clocks
        for (const auto& [peerId, counter] : clocks) {
            uint64_t otherCounter = other.get(peerId);
            if (counter > otherCounter) return false;  // Not happened-before
            if (counter < otherCounter) anyLess = true;
        }
        
        // Check keys in other.clocks that might be missing in this->clocks
        for (const auto& [peerId, otherCounter] : other.clocks) {
            if (clocks.find(peerId) == clocks.end()) {
                // Implicitly 0 in this clock
                // otherCounter is unsigned, so it's always >= 0
                if (otherCounter > 0) anyLess = true;
            }
        }
        
        return anyLess;
    }
    
    // Check if concurrent (neither happened-before the other)
    bool isConcurrentWith(const VectorClock& other) const {
        return !happensBefore(other) && !other.happensBefore(*this);
    }
    
    // Serialize to string format: "peer1:5,peer2:3,peer3:1"
    std::string serialize() const {
        std::string result;
        for (const auto& [peerId, counter] : clocks) {
            if (!result.empty()) result += ",";
            result += peerId + ":" + std::to_string(counter);
        }
        return result;
    }
    
    // Deserialize from string
    static VectorClock deserialize(const std::string& str) {
        VectorClock vc;
        size_t pos = 0;
        while (pos < str.length()) {
            size_t colon = str.find(':', pos);
            size_t comma = str.find(',', pos);
            if (colon == std::string::npos) break;
            
            std::string peerId = str.substr(pos, colon - pos);
            std::string counterStr = (comma != std::string::npos) 
                ? str.substr(colon + 1, comma - colon - 1)
                : str.substr(colon + 1);
            
            vc.clocks[peerId] = std::stoull(counterStr);
            pos = (comma != std::string::npos) ? comma + 1 : str.length();
        }
        return vc;
    }
};

/**
 * @brief Conflict detection and resolution engine
 */
class ConflictResolver {
public:
    /**
     * @brief Detect if incoming change conflicts with local state
     * @param path File path
     * @param localHash Local file SHA-256
     * @param remoteHash Remote file SHA-256
     * @param localTimestamp Local modification time (ms)
     * @param remoteTimestamp Remote modification time (ms)
     * @param localClock Local vector clock
     * @param remoteClock Remote vector clock
     * @return true if conflict detected
     */
    static bool detectConflict(
        const std::string& path,
        const std::string& localHash,
        const std::string& remoteHash,
        uint64_t localTimestamp,
        uint64_t remoteTimestamp,
        const VectorClock& localClock,
        const VectorClock& remoteClock
    );
    
    /**
     * @brief Resolve a conflict using specified strategy
     * @param conflict Conflict metadata
     * @param localPath Path to local file
     * @param remoteData Remote file data
     * @return true if resolution successful
     */
    static bool resolveConflict(
        FileConflict& conflict,
        const std::string& localPath,
        const std::vector<uint8_t>& remoteData
    );
    
    /**
     * @brief Apply NEWEST_WINS strategy
     */
    static bool applyNewestWins(
        const FileConflict& conflict,
        const std::string& localPath,
        const std::vector<uint8_t>& remoteData
    );
    
    /**
     * @brief Apply KEEP_BOTH strategy (create .conflict files)
     */
    static bool applyKeepBoth(
        const FileConflict& conflict,
        const std::string& localPath,
        const std::vector<uint8_t>& remoteData
    );
    
    /**
     * @brief Apply LARGEST_WINS strategy
     */
    static bool applyLargestWins(
        const FileConflict& conflict,
        const std::string& localPath,
        const std::vector<uint8_t>& remoteData
    );
    
    /**
     * @brief Generate conflict filename with timestamp
     * @param originalPath Original file path
     * @param suffix Conflict suffix (e.g., "local", "remote", "peer_12345")
     * @return Path like "file.conflict.local.txt"
     */
    static std::string generateConflictPath(
        const std::string& originalPath,
        const std::string& suffix
    );
};

} // namespace SentinelFS
