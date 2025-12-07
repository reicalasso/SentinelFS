#include "FileEntropyAnalyzer.h"
#include <cmath>
#include <array>
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace SentinelFS {

// Known high-entropy extensions (compressed, encrypted, binary)
static const std::vector<std::string> KNOWN_HIGH_ENTROPY_EXTENSIONS = {
    ".zip", ".gz", ".tar.gz", ".tgz", ".bz2", ".xz", ".7z", ".rar",
    ".jpg", ".jpeg", ".png", ".gif", ".webp", ".mp3", ".mp4", ".mkv",
    ".avi", ".mov", ".flac", ".aac", ".ogg", ".pdf", ".docx", ".xlsx",
    ".pptx", ".gpg", ".aes", ".enc"
};

// Typical entropy ranges for common file types
static const std::map<std::string, std::pair<double, double>> TYPICAL_ENTROPY_RANGES = {
    {".txt", {3.0, 5.5}},
    {".log", {3.5, 5.5}},
    {".c", {4.0, 5.5}},
    {".cpp", {4.0, 5.5}},
    {".h", {4.0, 5.5}},
    {".hpp", {4.0, 5.5}},
    {".py", {4.0, 5.5}},
    {".js", {4.0, 5.5}},
    {".ts", {4.0, 5.5}},
    {".html", {4.0, 5.5}},
    {".css", {4.0, 5.5}},
    {".json", {4.0, 6.0}},
    {".xml", {4.0, 5.5}},
    {".yaml", {3.5, 5.5}},
    {".yml", {3.5, 5.5}},
    {".md", {3.5, 5.5}},
    {".csv", {3.5, 5.5}},
    {".sql", {4.0, 5.5}},
    {".sh", {4.0, 5.5}},
    {".conf", {3.5, 5.5}},
    {".ini", {3.5, 5.0}},
    // Binary/compressed types expected to be high
    {".zip", {7.5, 8.0}},
    {".gz", {7.5, 8.0}},
    {".jpg", {7.5, 8.0}},
    {".png", {7.0, 8.0}},
    {".pdf", {6.5, 8.0}},
    {".exe", {5.5, 7.5}},
    {".dll", {5.5, 7.5}},
    {".so", {5.5, 7.5}},
};

FileEntropyAnalyzer::FileEntropyAnalyzer() = default;

FileEntropyAnalyzer::EntropyResult FileEntropyAnalyzer::analyzeFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    EntropyResult result;
    std::string extension = extractExtension(path);
    
    // Check if file exists and is readable
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        result.reason = "Could not open file";
        return result;
    }
    
    size_t fileSize = file.tellg();
    if (fileSize < MIN_ANALYZE_SIZE) {
        result.reason = "File too small for entropy analysis";
        return result;
    }
    
    file.seekg(0, std::ios::beg);
    
    // Read sample (up to MAX_ANALYZE_SIZE)
    size_t sampleSize = std::min(fileSize, MAX_ANALYZE_SIZE);
    std::vector<uint8_t> buffer(sampleSize);
    file.read(reinterpret_cast<char*>(buffer.data()), sampleSize);
    
    // Calculate entropy
    result.entropy = calculateEntropy(buffer.data(), buffer.size());
    analyzedFiles_++;
    
    // Check if high entropy
    result.isHighEntropy = result.entropy >= HIGH_ENTROPY_THRESHOLD;
    if (result.isHighEntropy) {
        highEntropyFiles_++;
    }
    
    // Check if encrypted-looking
    result.isEncryptedLooking = result.entropy >= ENCRYPTED_ENTROPY_MIN;
    
    // Get baseline for this file type
    auto baselineIt = baselines_.find(extension);
    if (baselineIt != baselines_.end() && baselineIt->second.sampleCount >= 5) {
        result.baselineEntropy = baselineIt->second.meanEntropy;
        
        // Check if anomalous compared to baseline
        double stdDev = baselineIt->second.stdDevEntropy;
        if (stdDev > 0.01) {
            double zScore = (result.entropy - result.baselineEntropy) / stdDev;
            result.isAnomalous = zScore > ANOMALY_SIGMA;
            
            if (result.isAnomalous) {
                std::ostringstream oss;
                oss << "Entropy " << std::fixed << std::setprecision(2) << result.entropy
                    << " is " << std::setprecision(1) << zScore << " sigma above baseline ("
                    << std::setprecision(2) << result.baselineEntropy << ")";
                result.reason = oss.str();
            }
        }
    } else {
        // No baseline yet, check against typical ranges
        auto rangeIt = TYPICAL_ENTROPY_RANGES.find(extension);
        if (rangeIt != TYPICAL_ENTROPY_RANGES.end()) {
            result.baselineEntropy = (rangeIt->second.first + rangeIt->second.second) / 2.0;
            
            // If entropy is way above typical maximum for this type
            if (result.entropy > rangeIt->second.second + 1.5) {
                result.isAnomalous = true;
                std::ostringstream oss;
                oss << "Entropy " << std::fixed << std::setprecision(2) << result.entropy
                    << " exceeds typical range for " << extension << " files ("
                    << rangeIt->second.first << "-" << rangeIt->second.second << ")";
                result.reason = oss.str();
            }
        }
    }
    
    // Special case: text-like files with high entropy are very suspicious
    if (extension == ".txt" || extension == ".log" || extension == ".md" ||
        extension == ".c" || extension == ".cpp" || extension == ".h" ||
        extension == ".py" || extension == ".js") {
        if (result.entropy > 7.0) {
            result.isAnomalous = true;
            result.reason = "Text file with unusually high entropy (possible encryption)";
        }
    }
    
    // Update baseline (only if not already flagged as anomalous)
    if (!result.isAnomalous) {
        updateBaseline(extension, result.entropy);
    }
    
    return result;
}

double FileEntropyAnalyzer::calculateEntropy(const uint8_t* data, size_t size) const {
    if (size == 0) return 0.0;
    
    // Count byte frequencies
    std::array<size_t, 256> freq{};
    for (size_t i = 0; i < size; i++) {
        freq[data[i]]++;
    }
    
    // Calculate Shannon entropy
    double entropy = 0.0;
    double sizeD = static_cast<double>(size);
    
    for (size_t i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            double p = static_cast<double>(freq[i]) / sizeD;
            entropy -= p * std::log2(p);
        }
    }
    
    return entropy;
}

void FileEntropyAnalyzer::recordBaseline(const std::string& extension, double entropy) {
    std::lock_guard<std::mutex> lock(mutex_);
    updateBaseline(extension, entropy);
}

FileEntropyAnalyzer::FileTypeBaseline FileEntropyAnalyzer::getBaseline(const std::string& extension) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = baselines_.find(extension);
    if (it != baselines_.end()) {
        return it->second;
    }
    return FileTypeBaseline{};
}

bool FileEntropyAnalyzer::isEntropyAnomalous(const std::string& extension, double entropy) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = baselines_.find(extension);
    if (it == baselines_.end() || it->second.sampleCount < 5) {
        // No baseline, check against typical
        auto rangeIt = TYPICAL_ENTROPY_RANGES.find(extension);
        if (rangeIt != TYPICAL_ENTROPY_RANGES.end()) {
            return entropy > rangeIt->second.second + 1.0;
        }
        return entropy > HIGH_ENTROPY_THRESHOLD;
    }
    
    const auto& baseline = it->second;
    if (baseline.stdDevEntropy < 0.01) return false;
    
    double zScore = (entropy - baseline.meanEntropy) / baseline.stdDevEntropy;
    return zScore > ANOMALY_SIGMA;
}

const std::vector<std::string>& FileEntropyAnalyzer::getKnownHighEntropyExtensions() {
    return KNOWN_HIGH_ENTROPY_EXTENSIONS;
}

std::pair<double, double> FileEntropyAnalyzer::getTypicalEntropyRange(const std::string& extension) {
    auto it = TYPICAL_ENTROPY_RANGES.find(extension);
    if (it != TYPICAL_ENTROPY_RANGES.end()) {
        return it->second;
    }
    // Default range for unknown types
    return {4.0, 7.0};
}

bool FileEntropyAnalyzer::saveBaselines(const std::string& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ofstream file(path);
    if (!file) return false;
    
    file << "# FileEntropyAnalyzer Baselines\n";
    file << "# extension,mean,stddev,samples\n";
    
    for (const auto& [ext, baseline] : baselines_) {
        file << ext << "," << baseline.meanEntropy << ","
             << baseline.stdDevEntropy << "," << baseline.sampleCount << "\n";
    }
    
    file << "# Stats: analyzed=" << analyzedFiles_ << ",high_entropy=" << highEntropyFiles_ << "\n";
    
    return true;
}

bool FileEntropyAnalyzer::loadBaselines(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(path);
    if (!file) return false;
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        std::string ext;
        char comma;
        FileTypeBaseline baseline;
        
        if (std::getline(iss, ext, ',') &&
            iss >> baseline.meanEntropy >> comma
                >> baseline.stdDevEntropy >> comma
                >> baseline.sampleCount) {
            baselines_[ext] = baseline;
        }
    }
    
    return true;
}

void FileEntropyAnalyzer::updateBaseline(const std::string& extension, double entropy) {
    auto& baseline = baselines_[extension];
    
    // Welford's online algorithm for mean and variance
    baseline.sampleCount++;
    double delta = entropy - baseline.meanEntropy;
    baseline.meanEntropy += delta / baseline.sampleCount;
    double delta2 = entropy - baseline.meanEntropy;
    
    if (baseline.sampleCount > 1) {
        double variance = (baseline.stdDevEntropy * baseline.stdDevEntropy) * (baseline.sampleCount - 2);
        variance += delta * delta2;
        variance /= (baseline.sampleCount - 1);
        baseline.stdDevEntropy = std::sqrt(std::max(0.0, variance));
    }
}

std::string FileEntropyAnalyzer::extractExtension(const std::string& path) const {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    
    // Handle double extensions like .tar.gz
    std::string stem = p.stem().string();
    size_t dotPos = stem.rfind('.');
    if (dotPos != std::string::npos) {
        std::string doubleExt = stem.substr(dotPos) + ext;
        if (doubleExt == ".tar.gz" || doubleExt == ".tar.bz2" || doubleExt == ".tar.xz") {
            return doubleExt;
        }
    }
    
    if (ext.empty()) return "[no-ext]";
    
    // Lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

} // namespace SentinelFS
