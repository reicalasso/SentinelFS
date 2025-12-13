/**
 * @file PinRotation.cpp
 * @brief Certificate pin rotation and backup verification functionality
 */

#include "TLSContext.h"
#include "Logger.h"
#include <algorithm>
#include <ctime>

namespace SentinelFS {

bool TLSContext::rotatePin(const std::string& hostname, 
                          const std::string& oldSpkiHash,
                          const std::string& newSpkiHash,
                          int validityDays) {
    auto& logger = Logger::instance();
    
    // Find the existing pin
    auto it = std::find_if(pins_.begin(), pins_.end(),
        [this, &hostname, &oldSpkiHash](const CertificatePin& pin) {
            return matchPattern(pin.hostname, hostname) && pin.spkiHash == oldSpkiHash;
        });
    
    if (it == pins_.end()) {
        logger.log(LogLevel::ERROR, "Cannot rotate: no existing pin found for " + hostname, "TLSContext");
        return false;
    }
    
    // Create backup of old pin
    CertificatePin backupPin = *it;
    backupPin.comment += " [BACKUP-" + std::to_string(time(nullptr)) + "]";
    backupPin.expiresAt = time(nullptr) + (validityDays * 24 * 3600); // Keep backup for N days
    
    // Update the existing pin
    it->spkiHash = newSpkiHash;
    it->comment += " [ROTATED-" + std::to_string(time(nullptr)) + "]";
    
    // Add backup pin
    pins_.push_back(backupPin);
    
    logger.log(LogLevel::INFO, "Rotated pin for " + hostname + ", backup valid for " + 
               std::to_string(validityDays) + " days", "TLSContext");
    
    return true;
}

bool TLSContext::verifyWithBackup(const std::string& hostname,
                                 const std::string& spkiHash,
                                 const std::string& fingerprint) {
    auto& logger = Logger::instance();
    time_t now = time(nullptr);
    
    // First try primary pins (non-backup)
    for (const auto& pin : pins_) {
        if (pin.expiresAt > 0 && pin.expiresAt < now) continue;
        if (pin.comment.find("[BACKUP-") != std::string::npos) continue; // Skip backups
        
        if (!matchPattern(pin.hostname, hostname)) continue;
        
        if (!pin.spkiHash.empty() && pin.spkiHash == spkiHash) {
            logger.log(LogLevel::DEBUG, "Primary pin matched for " + hostname, "TLSContext");
            return true;
        }
        
        if (!pin.fingerprint.empty() && pin.fingerprint == fingerprint) {
            logger.log(LogLevel::DEBUG, "Primary fingerprint matched for " + hostname, "TLSContext");
            return true;
        }
    }
    
    // If primary pins failed, try backup pins
    logger.log(LogLevel::WARN, "Primary pins failed for " + hostname + ", trying backup pins", "TLSContext");
    
    for (const auto& pin : pins_) {
        if (pin.expiresAt > 0 && pin.expiresAt < now) continue;
        if (pin.comment.find("[BACKUP-") == std::string::npos) continue; // Only backup pins
        
        if (!matchPattern(pin.hostname, hostname)) continue;
        
        if (!pin.spkiHash.empty() && pin.spkiHash == spkiHash) {
            logger.log(LogLevel::INFO, "Backup pin matched for " + hostname + " - certificate rotation in progress", "TLSContext");
            return true;
        }
        
        if (!pin.fingerprint.empty() && pin.fingerprint == fingerprint) {
            logger.log(LogLevel::INFO, "Backup fingerprint matched for " + hostname + " - certificate rotation in progress", "TLSContext");
            return true;
        }
    }
    
    logger.log(LogLevel::ERROR, "All pins (primary and backup) failed for " + hostname, "TLSContext");
    return false;
}

std::vector<TLSContext::CertificatePin> TLSContext::getExpiringPins(int daysThreshold) {
    std::vector<CertificatePin> expiring;
    time_t now = time(nullptr);
    time_t threshold = now + (daysThreshold * 24 * 3600);
    
    for (const auto& pin : pins_) {
        if (pin.expiresAt > 0 && pin.expiresAt < threshold && pin.expiresAt > now) {
            expiring.push_back(pin);
        }
    }
    
    return expiring;
}

bool TLSContext::cleanupExpiredPins() {
    auto& logger = Logger::instance();
    time_t now = time(nullptr);
    
    auto originalSize = pins_.size();
    pins_.erase(
        std::remove_if(pins_.begin(), pins_.end(),
            [now](const CertificatePin& pin) {
                return pin.expiresAt > 0 && pin.expiresAt < now;
            }),
        pins_.end()
    );
    
    auto removed = originalSize - pins_.size();
    if (removed > 0) {
        logger.log(LogLevel::INFO, "Cleaned up " + std::to_string(removed) + " expired pins", "TLSContext");
    }
    
    return removed > 0;
}

} // namespace SentinelFS
