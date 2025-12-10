#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <fstream>

namespace SentinelFS {

/**
 * @brief File entropy analyzer for ransomware detection
 * 
 * Ransomware typically encrypts files, resulting in high entropy (close to 8 bits/byte).
 * This analyzer:
 * - Calculates Shannon entropy of file contents
 * - Tracks baseline entropy per file type
 * - Detects sudden entropy increases (potential encryption)
 * - Identifies encrypted-looking files
 */
class FileEntropyAnalyzer {
public:
    struct EntropyResult {
        double entropy{0.0};               // Shannon entropy (0-8 bits)
        bool isHighEntropy{false};         // Above threshold
        bool isEncryptedLooking{false};    // Matches encrypted file characteristics
        bool isAnomalous{false};           // Significantly higher than baseline
        double baselineEntropy{0.0};       // Expected entropy for this file type
        std::string reason;
    };

    struct FileTypeBaseline {
        double meanEntropy{0.0};
        double stdDevEntropy{0.0};
        int sampleCount{0};
    };

    FileEntropyAnalyzer();

    /**
     * @brief Analyze a file's entropy
     * @param path Path to the file
     * @return Entropy analysis result
     */
    EntropyResult analyzeFile(const std::string& path);

    /**
     * @brief Analyze raw data entropy
     * @param data Data buffer
     * @param size Data size
     * @return Shannon entropy (0-8 bits)
     */
    double calculateEntropy(const uint8_t* data, size_t size) const;

    /**
     * @brief Record file entropy for baseline learning
     */
    void recordBaseline(const std::string& extension, double entropy);

    /**
     * @brief Get baseline for a file type
     */
    FileTypeBaseline getBaseline(const std::string& extension) const;

    /**
     * @brief Check if entropy is anomalous for this file type
     */
    bool isEntropyAnomalous(const std::string& extension, double entropy) const;

    /**
     * @brief Get list of high-entropy file extensions (likely already encrypted)
     */
    static const std::vector<std::string>& getKnownHighEntropyExtensions();

    /**
     * @brief Get typical entropy range for a file type
     */
    static std::pair<double, double> getTypicalEntropyRange(const std::string& extension);

    /**
     * @brief Save baselines to file
     */
    bool saveBaselines(const std::string& path) const;

    /**
     * @brief Load baselines from file
     */
    bool loadBaselines(const std::string& path);

    /**
     * @brief Get statistics
     */
    size_t getAnalyzedFileCount() const { return analyzedFiles_; }
    size_t getHighEntropyCount() const { return highEntropyFiles_; }

private:
    mutable std::mutex mutex_;
    
    // File type baselines (learned over time)
    std::map<std::string, FileTypeBaseline> baselines_;
    
    // Statistics
    size_t analyzedFiles_{0};
    size_t highEntropyFiles_{0};
    
    // Configuration
    static constexpr double HIGH_ENTROPY_THRESHOLD = 7.5;     // bits (8 max)
    static constexpr double ENCRYPTED_ENTROPY_MIN = 7.8;      // Very close to max
    static constexpr double ANOMALY_SIGMA = 3.0;              // Standard deviations
    static constexpr size_t MAX_ANALYZE_SIZE = 1024 * 1024;   // 1MB sample
    static constexpr size_t MIN_ANALYZE_SIZE = 256;           // Minimum for meaningful entropy
    
    // Helper functions
    std::string extractExtension(const std::string& path) const;
    void updateBaseline(const std::string& extension, double entropy);
};

} // namespace SentinelFS
