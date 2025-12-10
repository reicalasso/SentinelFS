/**
 * @file PinManager.cpp
 * @brief TLSContext certificate pinning functionality
 */

#include "TLSContext.h"
#include "Logger.h"
#include "Crypto.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace SentinelFS {

void TLSContext::setPinningPolicy(PinningPolicy policy) {
    pinningPolicy_ = policy;
}

void TLSContext::addPin(const CertificatePin& pin) {
    // Remove existing pin for same hostname
    removePin(pin.hostname);
    pins_.push_back(pin);
}

bool TLSContext::removePin(const std::string& hostname) {
    auto it = std::find_if(pins_.begin(), pins_.end(),
        [&hostname](const CertificatePin& p) {
            return p.hostname == hostname;
        });
    
    if (it != pins_.end()) {
        pins_.erase(it);
        return true;
    }
    return false;
}

bool TLSContext::loadPins(const std::string& path) {
    auto& logger = Logger::instance();
    
    std::ifstream file(path);
    if (!file.is_open()) {
        lastError_ = "Failed to open pins file: " + path;
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    // Simple JSON-like parsing (production should use a proper JSON library)
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    // Parse entries in format: hostname:spkiHash:fingerprint:expires
    std::istringstream iss(content);
    std::string line;
    
    pins_.clear();
    while (std::getline(iss, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream lineStream(line);
        CertificatePin pin;
        
        std::getline(lineStream, pin.hostname, ':');
        std::getline(lineStream, pin.spkiHash, ':');
        std::getline(lineStream, pin.fingerprint, ':');
        
        std::string expires;
        std::getline(lineStream, expires, ':');
        if (!expires.empty()) {
            pin.expiresAt = std::stol(expires);
        }
        
        std::getline(lineStream, pin.comment);
        
        if (!pin.hostname.empty() && (!pin.spkiHash.empty() || !pin.fingerprint.empty())) {
            pins_.push_back(pin);
        }
    }
    
    logger.log(LogLevel::INFO, "Loaded " + std::to_string(pins_.size()) + " certificate pins", "TLSContext");
    return true;
}

bool TLSContext::savePins(const std::string& path) {
    auto& logger = Logger::instance();
    
    std::ofstream file(path);
    if (!file.is_open()) {
        lastError_ = "Failed to create pins file: " + path;
        logger.log(LogLevel::ERROR, lastError_, "TLSContext");
        return false;
    }
    
    file << "# SentinelFS Certificate Pins\n";
    file << "# Format: hostname:spkiHash:fingerprint:expires:comment\n\n";
    
    for (const auto& pin : pins_) {
        file << pin.hostname << ":"
             << pin.spkiHash << ":"
             << pin.fingerprint << ":"
             << pin.expiresAt << ":"
             << pin.comment << "\n";
    }
    
    logger.log(LogLevel::DEBUG, "Saved " + std::to_string(pins_.size()) + " certificate pins", "TLSContext");
    return true;
}

std::vector<TLSContext::CertificatePin> TLSContext::getPins() const {
    return pins_;
}

bool TLSContext::checkPin(const std::string& hostname, 
                          const std::string& spkiHash,
                          const std::string& fingerprint) {
    auto& logger = Logger::instance();
    time_t now = time(nullptr);
    
    for (const auto& pin : pins_) {
        // Skip expired pins
        if (pin.expiresAt > 0 && pin.expiresAt < now) {
            continue;
        }
        
        // Check hostname match
        if (!matchPattern(pin.hostname, hostname) && pin.hostname != "*") {
            continue;
        }
        
        // Check SPKI hash or fingerprint
        if (!pin.spkiHash.empty() && pin.spkiHash == spkiHash) {
            logger.log(LogLevel::DEBUG, "SPKI pin matched for " + hostname, "TLSContext");
            return true;
        }
        
        if (!pin.fingerprint.empty() && pin.fingerprint == fingerprint) {
            logger.log(LogLevel::DEBUG, "Fingerprint pin matched for " + hostname, "TLSContext");
            return true;
        }
    }
    
    // Handle TOFU policy
    if (pinningPolicy_ == PinningPolicy::TRUST_ON_FIRST_USE) {
        handleTOFU(hostname, spkiHash);
        return true;  // Trust on first use
    }
    
    return (pinningPolicy_ == PinningPolicy::NONE);
}

void TLSContext::handleTOFU(const std::string& hostname, const std::string& spkiHash) {
    auto& logger = Logger::instance();
    
    // Add new pin for first-seen certificate
    CertificatePin pin;
    pin.hostname = hostname;
    pin.spkiHash = spkiHash;
    pin.comment = "Auto-pinned via TOFU";
    
    addPin(pin);
    
    logger.log(LogLevel::INFO, "TOFU: Pinned certificate for " + hostname, "TLSContext");
    
    // Save pins if path is configured
    if (!tofuStorePath_.empty()) {
        savePins(tofuStorePath_);
    }
}

} // namespace SentinelFS
