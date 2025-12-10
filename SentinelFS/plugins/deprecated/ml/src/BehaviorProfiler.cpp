#include "BehaviorProfiler.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <filesystem>

namespace SentinelFS {

BehaviorProfiler::BehaviorProfiler()
    : profileStartTime_(std::chrono::steady_clock::now()) {
}

void BehaviorProfiler::recordActivity(const std::string& action, const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Add to recent activities
    recentActivities_.push_back({now, action, path});
    pruneOldActivities();
    
    // Update profiles
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    int currentHour = tm->tm_hour;
    
    // Calculate current activity rate
    double rate = getCurrentActivityRate();
    updateHourlyProfile(currentHour, rate);
    
    // Update directory profile
    std::string directory = extractDirectory(path);
    updateDirectoryProfile(directory);
    
    // Update file type profile
    std::string extension = extractExtension(path);
    updateFileTypeProfile(extension, action);
    
    totalActivities_++;
}

BehaviorProfiler::AnomalyResult BehaviorProfiler::checkForAnomaly() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AnomalyResult result;
    
    if (!isProfileReady()) {
        result.isAnomaly = false;
        result.reason = "Profile not ready (learning mode)";
        return result;
    }
    
    // Get current hour
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    int currentHour = tm->tm_hour;
    
    // Check activity rate anomaly
    double currentRate = getCurrentActivityRate();
    auto hourIt = hourlyProfiles_.find(currentHour);
    
    if (hourIt != hourlyProfiles_.end() && hourIt->second.sampleCount >= 10) {
        const auto& profile = hourIt->second;
        
        if (profile.stdDevActivityRate > 0.001) {  // Avoid division by zero
            double zScore = calculateZScore(currentRate, profile.meanActivityRate, profile.stdDevActivityRate);
            
            if (std::abs(zScore) > ANOMALY_THRESHOLD_SIGMA) {
                result.isAnomaly = true;
                result.score = std::min(1.0, std::abs(zScore) / 5.0);  // Normalize to 0-1
                result.category = "RATE";
                
                if (currentRate > profile.meanActivityRate) {
                    result.reason = "Activity rate (" + std::to_string(static_cast<int>(currentRate)) + 
                                   "/min) is " + std::to_string(static_cast<int>(zScore)) + 
                                   " sigma above normal for this hour";
                } else {
                    result.reason = "Activity rate unusually low for this hour";
                }
                return result;
            }
        }
    }
    
    // Check for unusual directory access (accessing rarely-used directories)
    if (!recentActivities_.empty()) {
        const auto& lastActivity = recentActivities_.back();
        std::string lastDir = extractDirectory(lastActivity.path);
        
        auto dirIt = directoryProfiles_.find(lastDir);
        if (dirIt != directoryProfiles_.end()) {
            // If this directory is rarely accessed but suddenly getting activity
            if (dirIt->second.normalFrequency < 0.01 && dirIt->second.accessCount > 5) {
                auto timeSinceLastAccess = std::chrono::duration_cast<std::chrono::hours>(
                    std::chrono::steady_clock::now() - dirIt->second.lastAccess
                ).count();
                
                if (timeSinceLastAccess > 24 * 7) {  // Not accessed in a week
                    result.isAnomaly = true;
                    result.score = 0.5;
                    result.category = "DIRECTORY";
                    result.reason = "Unusual access to rarely-used directory: " + lastDir;
                    return result;
                }
            }
        }
    }
    
    // Check for unusual file type ratio shifts
    // (e.g., suddenly lots of modifications to file types that are usually read-only)
    double totalTypeActivities = 0;
    for (const auto& [ext, profile] : fileTypeProfiles_) {
        totalTypeActivities += profile.createCount + profile.modifyCount + profile.deleteCount;
    }
    
    if (totalTypeActivities > MIN_SAMPLES_FOR_PROFILE) {
        for (const auto& [ext, profile] : fileTypeProfiles_) {
            double typeTotal = profile.createCount + profile.modifyCount + profile.deleteCount;
            double currentRatio = typeTotal / totalTypeActivities;
            
            // If this type's activity is 5x its normal ratio
            if (profile.normalRatio > 0.001 && currentRatio > profile.normalRatio * 5) {
                result.isAnomaly = true;
                result.score = 0.6;
                result.category = "PATTERN";
                result.reason = "Unusual activity spike for file type: " + ext;
                return result;
            }
        }
    }
    
    result.isAnomaly = false;
    result.score = 0.0;
    return result;
}

double BehaviorProfiler::getCurrentActivityRate() const {
    if (recentActivities_.empty()) return 0.0;
    
    auto now = std::chrono::steady_clock::now();
    int count = 0;
    
    for (const auto& activity : recentActivities_) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - activity.timestamp
        ).count();
        
        if (age <= ACTIVITY_WINDOW_SECONDS) {
            count++;
        }
    }
    
    // Return rate per minute
    return static_cast<double>(count) * (60.0 / ACTIVITY_WINDOW_SECONDS);
}

double BehaviorProfiler::getLearningProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Consider profile ready when we have enough samples
    double progress = static_cast<double>(totalActivities_) / MIN_SAMPLES_FOR_PROFILE;
    return std::min(1.0, progress);
}

bool BehaviorProfiler::isProfileReady() const {
    return totalActivities_ >= MIN_SAMPLES_FOR_PROFILE;
}

bool BehaviorProfiler::saveProfile(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(path);
    if (!file) return false;
    
    // Save hourly profiles
    file << "[HOURLY]\n";
    for (const auto& [hour, profile] : hourlyProfiles_) {
        file << hour << "," << profile.meanActivityRate << ","
             << profile.stdDevActivityRate << "," << profile.sampleCount << "\n";
    }
    
    // Save directory profiles
    file << "[DIRECTORIES]\n";
    for (const auto& [dir, profile] : directoryProfiles_) {
        file << dir << "," << profile.accessCount << "," << profile.normalFrequency << "\n";
    }
    
    // Save file type profiles
    file << "[FILETYPES]\n";
    for (const auto& [ext, profile] : fileTypeProfiles_) {
        file << ext << "," << profile.createCount << "," << profile.modifyCount << ","
             << profile.deleteCount << "," << profile.normalRatio << "\n";
    }
    
    file << "[STATS]\n";
    file << totalActivities_ << "\n";
    
    return true;
}

bool BehaviorProfiler::loadProfile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(path);
    if (!file) return false;
    
    std::string line;
    std::string section;
    
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        if (line[0] == '[') {
            section = line;
            continue;
        }
        
        std::istringstream iss(line);
        
        if (section == "[HOURLY]") {
            int hour;
            char comma;
            HourlyProfile profile;
            if (iss >> hour >> comma >> profile.meanActivityRate >> comma 
                    >> profile.stdDevActivityRate >> comma >> profile.sampleCount) {
                hourlyProfiles_[hour] = profile;
            }
        } else if (section == "[DIRECTORIES]") {
            std::string dir;
            char comma;
            DirectoryProfile profile;
            if (std::getline(iss, dir, ',') && 
                iss >> profile.accessCount >> comma >> profile.normalFrequency) {
                directoryProfiles_[dir] = profile;
            }
        } else if (section == "[FILETYPES]") {
            std::string ext;
            char comma;
            FileTypeProfile profile;
            if (std::getline(iss, ext, ',') &&
                iss >> profile.createCount >> comma >> profile.modifyCount >> comma
                    >> profile.deleteCount >> comma >> profile.normalRatio) {
                fileTypeProfiles_[ext] = profile;
            }
        } else if (section == "[STATS]") {
            iss >> totalActivities_;
        }
    }
    
    return true;
}

void BehaviorProfiler::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    hourlyProfiles_.clear();
    directoryProfiles_.clear();
    fileTypeProfiles_.clear();
    recentActivities_.clear();
    totalActivities_ = 0;
    profileStartTime_ = std::chrono::steady_clock::now();
}

size_t BehaviorProfiler::getProfiledHours() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hourlyProfiles_.size();
}

void BehaviorProfiler::updateHourlyProfile(int hour, double rate) {
    auto& profile = hourlyProfiles_[hour];
    
    // Online mean and variance update (Welford's algorithm)
    profile.sampleCount++;
    double delta = rate - profile.meanActivityRate;
    profile.meanActivityRate += delta / profile.sampleCount;
    double delta2 = rate - profile.meanActivityRate;
    
    // Update variance estimation
    if (profile.sampleCount > 1) {
        double variance = (profile.stdDevActivityRate * profile.stdDevActivityRate) * (profile.sampleCount - 2);
        variance += delta * delta2;
        variance /= (profile.sampleCount - 1);
        profile.stdDevActivityRate = std::sqrt(std::max(0.0, variance));
    }
}

void BehaviorProfiler::updateDirectoryProfile(const std::string& directory) {
    auto& profile = directoryProfiles_[directory];
    profile.accessCount++;
    profile.lastAccess = std::chrono::steady_clock::now();
    
    // Update frequency (simple running average)
    double totalAccesses = 0;
    for (const auto& [dir, p] : directoryProfiles_) {
        totalAccesses += p.accessCount;
    }
    
    for (auto& [dir, p] : directoryProfiles_) {
        p.normalFrequency = static_cast<double>(p.accessCount) / totalAccesses;
    }
}

void BehaviorProfiler::updateFileTypeProfile(const std::string& extension, const std::string& action) {
    auto& profile = fileTypeProfiles_[extension];
    
    if (action == "CREATE") {
        profile.createCount++;
    } else if (action == "MODIFY") {
        profile.modifyCount++;
    } else if (action == "DELETE") {
        profile.deleteCount++;
    }
    
    // Update ratios
    double totalActivities = 0;
    for (const auto& [ext, p] : fileTypeProfiles_) {
        totalActivities += p.createCount + p.modifyCount + p.deleteCount;
    }
    
    for (auto& [ext, p] : fileTypeProfiles_) {
        double typeTotal = p.createCount + p.modifyCount + p.deleteCount;
        p.normalRatio = typeTotal / totalActivities;
    }
}

std::string BehaviorProfiler::extractExtension(const std::string& path) const {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    if (ext.empty()) return "[no-ext]";
    return ext;
}

std::string BehaviorProfiler::extractDirectory(const std::string& path) const {
    std::filesystem::path p(path);
    return p.parent_path().string();
}

double BehaviorProfiler::calculateZScore(double value, double mean, double stdDev) const {
    if (stdDev < 0.001) return 0.0;
    return (value - mean) / stdDev;
}

void BehaviorProfiler::pruneOldActivities() {
    auto now = std::chrono::steady_clock::now();
    
    while (!recentActivities_.empty()) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - recentActivities_.front().timestamp
        ).count();
        
        if (age > ACTIVITY_WINDOW_SECONDS * 2) {
            recentActivities_.pop_front();
        } else {
            break;
        }
    }
    
    // Also limit by size
    while (recentActivities_.size() > MAX_RECENT_ACTIVITIES) {
        recentActivities_.pop_front();
    }
}

} // namespace SentinelFS
