#include "ml_data_generator.hpp"
#include <iostream>
#include <iomanip>

std::mt19937 MLDataGenerator::rng(std::chrono::steady_clock::now().time_since_epoch().count());

bool MLDataGenerator::generateAnomalyTrainingData(const std::string& outputPath, size_t sampleCount) {
    std::ofstream file(outputPath);
    if (!file) {
        return false;
    }
    
    // Write CSV header
    file << "hour,file_size,access_frequency,latency,bandwidth,is_anomaly\n";
    
    std::uniform_real_distribution<float> hourDist(0.0, 23.9);
    std::uniform_real_distribution<float> sizeDist(0.1, 1000.0);  // File size in MB
    std::uniform_real_distribution<float> freqDist(0.0, 1.0);    // Access frequency
    std::uniform_real_distribution<float> latencyDist(10.0, 200.0); // Latency in ms
    std::uniform_real_distribution<float> bandwidthDist(1.0, 100.0); // Bandwidth in Mbps
    
    for (size_t i = 0; i < sampleCount; ++i) {
        bool isAnomaly = false;
        float hour = hourDist(rng);
        float fileSize = sizeDist(rng);
        float frequency = freqDist(rng);
        float latency = latencyDist(rng);
        float bandwidth = bandwidthDist(rng);
        
        // Create some anomalies
        if (i % 10 == 0) {  // Every 10th sample is an anomaly
            isAnomaly = true;
            
            // Anomalies often happen during off-hours or with large files
            if (i % 30 == 0) {
                // Very large file transfer
                fileSize = 500.0 + sizeDist(rng) * 5;  // Much larger files
            } else if (i % 20 == 0) {
                // Off-hour access
                hour = (hour < 6.0) ? hour : (22.0 + (hourDist(rng) / 24.0) * 2.0);
            } else {
                // High frequency access
                frequency = 0.8 + (freqDist(rng) * 0.2);  // Very frequent access
            }
        }
        
        file << std::fixed << std::setprecision(2) 
             << hour << "," << fileSize << "," << frequency << "," 
             << latency << "," << bandwidth << "," << (isAnomaly ? "1" : "0") << "\n";
    }
    
    file.close();
    return true;
}

bool MLDataGenerator::generatePredictionTrainingData(const std::string& outputPath, size_t sampleCount) {
    std::ofstream file(outputPath);
    if (!file) {
        return false;
    }
    
    // Write CSV header
    file << "hour,day_of_week,user_id,file_id,access_probability\n";
    
    std::uniform_real_distribution<float> hourDist(0.0, 23.9);
    std::uniform_int_distribution<int> dayDist(0, 6);     // Day of week (0-6)
    std::uniform_int_distribution<int> userDist(1, 100);   // User IDs
    std::uniform_int_distribution<int> fileDist(1, 1000); // File IDs
    
    for (size_t i = 0; i < sampleCount; ++i) {
        float hour = hourDist(rng);
        int dayOfWeek = dayDist(rng);
        int userId = userDist(rng);
        int fileId = fileDist(rng);
        
        // Calculate access probability based on time patterns
        float probability = 0.1;  // Base probability
        
        // Higher during work hours (9-17)
        if (hour >= 9.0 && hour <= 17.0) {
            probability += 0.4;
        }
        
        // Slightly higher during typical computer usage hours
        if ((hour >= 18.0 && hour <= 22.0) || (hour >= 6.0 && hour <= 9.0)) {
            probability += 0.2;
        }
        
        // Weekdays slightly higher than weekends for business files
        if (dayOfWeek >= 1 && dayOfWeek <= 5) {
            probability += 0.1;
        }
        
        // Add some randomness
        std::uniform_real_distribution<float> noise(-0.1, 0.1);
        probability += noise(rng);
        
        // Clamp to 0-1 range
        probability = std::max(0.0f, std::min(1.0f, probability));
        
        file << std::fixed << std::setprecision(2) 
             << hour << "," << dayOfWeek << "," << userId << "," 
             << fileId << "," << probability << "\n";
    }
    
    file.close();
    return true;
}

bool MLDataGenerator::generateNetworkOptimizationData(const std::string& outputPath, size_t sampleCount) {
    std::ofstream file(outputPath);
    if (!file) {
        return false;
    }
    
    // Write CSV header
    file << "latency,bandwidth,packet_loss,stability,optimize_gain\n";
    
    std::uniform_real_distribution<float> latencyDist(10.0, 300.0);    // Latency in ms
    std::uniform_real_distribution<float> bandwidthDist(0.5, 100.0);   // Bandwidth in Mbps
    std::uniform_real_distribution<float> lossDist(0.0, 0.5);         // Packet loss (0-50%)
    std::uniform_real_distribution<float> stabDist(0.5, 1.0);        // Stability (0.5-1.0)
    
    for (size_t i = 0; i < sampleCount; ++i) {
        float latency = latencyDist(rng);
        float bandwidth = bandwidthDist(rng);
        float packetLoss = lossDist(rng);
        float stability = stabDist(rng);
        
        // Calculate optimization gain based on network quality
        float gain = 0.0;
        
        // Higher latency = more potential for optimization
        gain += std::min(0.5, latency / 200.0);
        
        // Lower bandwidth = more potential for optimization
        gain += std::min(0.3, (100.0 - bandwidth) / 100.0);
        
        // Higher packet loss = more potential for optimization
        gain += packetLoss * 0.2;
        
        // Lower stability = more potential for optimization
        gain += (1.0 - stability) * 0.2;
        
        // Clamp to 0-1 range
        gain = std::min(1.0f, gain);
        
        file << std::fixed << std::setprecision(2) 
             << latency << "," << bandwidth << "," << packetLoss << "," 
             << stability << "," << gain << "\n";
    }
    
    file.close();
    return true;
}

std::vector<float> MLDataGenerator::generateSampleFeatures(bool isAnomaly) {
    std::vector<float> features;
    
    std::uniform_real_distribution<float> hourDist(0.0, 23.9);
    std::uniform_real_distribution<float> sizeDist(0.1, 1000.0);
    std::uniform_real_distribution<float> freqDist(0.0, 1.0);
    std::uniform_real_distribution<float> latencyDist(10.0, 200.0);
    std::uniform_real_distribution<float> bandwidthDist(1.0, 100.0);
    
    if (isAnomaly) {
        // Generate features that would likely be classified as anomalies
        features.push_back(hourDist(rng) < 6.0 ? hourDist(rng) : (22.0 + hourDist(rng) * 0.1));  // Off-hour access
        features.push_back(100.0 + sizeDist(rng) * 10);  // Large file size
        features.push_back(0.8 + freqDist(rng) * 0.2);  // High frequency
        features.push_back(50.0 + latencyDist(rng));  // High latency
        features.push_back(1.0 + bandwidthDist(rng) * 0.5);  // Low bandwidth
    } else {
        // Generate normal features
        features.push_back(9.0 + hourDist(rng) * 0.5);  // Typical work hours
        features.push_back(sizeDist(rng));  // Normal file size
        features.push_back(freqDist(rng));  // Normal frequency
        features.push_back(20.0 + latencyDist(rng) * 0.3);  // Normal latency
        features.push_back(10.0 + bandwidthDist(rng));  // Normal bandwidth
    }
    
    return features;
}

std::vector<float> MLDataGenerator::generateSampleNetworkFeatures() {
    std::vector<float> features;
    
    std::uniform_real_distribution<float> latencyDist(10.0, 300.0);
    std::uniform_real_distribution<float> bandwidthDist(0.5, 100.0);
    std::uniform_real_distribution<float> lossDist(0.0, 0.5);
    std::uniform_real_distribution<float> stabDist(0.5, 1.0);
    
    features.push_back(latencyDist(rng));     // Latency in ms
    features.push_back(bandwidthDist(rng));  // Bandwidth in Mbps
    features.push_back(lossDist(rng));       // Packet loss rate
    features.push_back(stabDist(rng));       // Connection stability
    
    return features;
}

std::vector<float> MLDataGenerator::generateSampleAccessPatternFeatures() {
    std::vector<float> features;
    
    std::uniform_real_distribution<float> freqDist(0.0, 1.0);
    std::uniform_real_distribution<float> timeDist(0.0, 23.9);
    std::uniform_real_distribution<float> sizeDist(0.1, 1000.0);
    
    features.push_back(freqDist(rng));    // Access frequency (0-1)
    features.push_back(timeDist(rng));    // Last access time (hour)
    features.push_back(sizeDist(rng));    // File size (MB)
    features.push_back(freqDist(rng));    // Regularity of access (0-1)
    features.push_back(1.0f + freqDist(rng)); // File importance factor
    
    return features;
}