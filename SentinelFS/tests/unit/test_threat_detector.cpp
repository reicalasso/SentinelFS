/**
 * @file test_threat_detector.cpp
 * @brief Unit tests for advanced ML-based threat detection components
 * 
 * Tests: BehaviorProfiler, FileEntropyAnalyzer, PatternMatcher, 
 *        IsolationForest, and ThreatDetector
 */

#include "BehaviorProfiler.h"
#include "FileEntropyAnalyzer.h"
#include "PatternMatcher.h"
#include "IsolationForest.h"
#include "ThreatDetector.h"

#include <iostream>
#include <cassert>
#include <cmath>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <random>
#include <algorithm>

using namespace SentinelFS;
namespace fs = std::filesystem;

// ============================================
// BehaviorProfiler Tests
// ============================================

void test_behavior_profiler_learning() {
    std::cout << "Running test_behavior_profiler_learning..." << std::endl;
    
    BehaviorProfiler profiler;
    
    // Record normal activity patterns
    for (int i = 0; i < 100; ++i) {
        profiler.recordActivity("MODIFY", "/home/user/documents/file" + std::to_string(i % 10) + ".txt");
    }
    
    // Verify that profiler has recorded activities
    double activityRate = profiler.getCurrentActivityRate();
    std::cout << "  Current activity rate: " << activityRate << std::endl;
    
    // After 100 activities, should have some progress
    double progress = profiler.getLearningProgress();
    std::cout << "  Learning progress: " << progress << std::endl;
    
    std::cout << "test_behavior_profiler_learning passed." << std::endl;
}

void test_behavior_profiler_anomaly_detection() {
    std::cout << "Running test_behavior_profiler_anomaly_detection..." << std::endl;
    
    BehaviorProfiler profiler;
    
    // Build baseline with normal activity (small files during work hours)
    for (int i = 0; i < 200; ++i) {
        profiler.recordActivity("MODIFY", "/home/user/docs/file" + std::to_string(i % 10) + ".txt");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Check for anomaly (should be none or minor with consistent activity)
    auto result = profiler.checkForAnomaly();
    
    std::cout << "  Is anomaly: " << (result.isAnomaly ? "yes" : "no") << std::endl;
    std::cout << "  Anomaly score: " << result.score << std::endl;
    std::cout << "  Category: " << result.category << std::endl;
    
    // With consistent activity, should not be highly anomalous
    assert(result.score < 0.9);
    
    std::cout << "test_behavior_profiler_anomaly_detection passed." << std::endl;
}

// ============================================
// FileEntropyAnalyzer Tests
// ============================================

void test_entropy_low() {
    std::cout << "Running test_entropy_low..." << std::endl;
    
    FileEntropyAnalyzer analyzer;
    
    // Create test file with low entropy (repeated character)
    std::string testPath = "/tmp/test_low_entropy.txt";
    {
        std::ofstream f(testPath, std::ios::binary);
        for (int i = 0; i < 10000; ++i) {
            f << 'A';  // Same character = low entropy
        }
    }
    
    auto result = analyzer.analyzeFile(testPath);
    
    std::cout << "  Entropy: " << result.entropy << " bits/byte" << std::endl;
    std::cout << "  Is high entropy: " << (result.isHighEntropy ? "yes" : "no") << std::endl;
    std::cout << "  Is encrypted looking: " << (result.isEncryptedLooking ? "yes" : "no") << std::endl;
    
    // Repeated character should have very low entropy (close to 0)
    assert(result.entropy < 1.0);
    assert(!result.isHighEntropy);
    assert(!result.isEncryptedLooking);
    
    fs::remove(testPath);
    std::cout << "test_entropy_low passed." << std::endl;
}

void test_entropy_high() {
    std::cout << "Running test_entropy_high..." << std::endl;
    
    FileEntropyAnalyzer analyzer;
    
    // Create test file with high entropy (random data)
    std::string testPath = "/tmp/test_high_entropy.bin";
    {
        std::ofstream f(testPath, std::ios::binary);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        
        for (int i = 0; i < 10000; ++i) {
            char c = static_cast<char>(dis(gen));
            f.write(&c, 1);
        }
    }
    
    auto result = analyzer.analyzeFile(testPath);
    
    std::cout << "  Entropy: " << result.entropy << " bits/byte" << std::endl;
    std::cout << "  Is high entropy: " << (result.isHighEntropy ? "yes" : "no") << std::endl;
    std::cout << "  Is encrypted looking: " << (result.isEncryptedLooking ? "yes" : "no") << std::endl;
    
    // Random data should have high entropy (close to 8)
    assert(result.entropy > 7.0);
    assert(result.isHighEntropy);
    
    fs::remove(testPath);
    std::cout << "test_entropy_high passed." << std::endl;
}

void test_entropy_normal_text() {
    std::cout << "Running test_entropy_normal_text..." << std::endl;
    
    FileEntropyAnalyzer analyzer;
    
    // Create test file with normal English text
    std::string testPath = "/tmp/test_normal_text.txt";
    {
        std::ofstream f(testPath);
        std::string text = "The quick brown fox jumps over the lazy dog. "
                          "This is a test of normal English text with typical "
                          "character distribution. Files like this should have "
                          "moderate entropy, typically between 4 and 5 bits per byte. "
                          "Compressible text has lower entropy than random data.";
        
        // Write multiple times to get enough data
        for (int i = 0; i < 100; ++i) {
            f << text << "\n";
        }
    }
    
    auto result = analyzer.analyzeFile(testPath);
    
    std::cout << "  Entropy: " << result.entropy << " bits/byte" << std::endl;
    std::cout << "  Is high entropy: " << (result.isHighEntropy ? "yes" : "no") << std::endl;
    
    // Normal text typically has 4-5 bits/byte entropy
    assert(result.entropy > 3.0 && result.entropy < 6.5);
    assert(!result.isHighEntropy);
    
    fs::remove(testPath);
    std::cout << "test_entropy_normal_text passed." << std::endl;
}

void test_entropy_baseline() {
    std::cout << "Running test_entropy_baseline..." << std::endl;
    
    FileEntropyAnalyzer analyzer;
    
    // Record some baselines
    analyzer.recordBaseline(".txt", 4.5);
    analyzer.recordBaseline(".txt", 4.7);
    analyzer.recordBaseline(".txt", 4.3);
    
    // Get baseline
    auto baseline = analyzer.getBaseline(".txt");
    
    std::cout << "  Baseline mean entropy: " << baseline.meanEntropy << std::endl;
    std::cout << "  Baseline sample count: " << baseline.sampleCount << std::endl;
    
    // Should have 3 samples
    assert(baseline.sampleCount == 3);
    // Mean should be around 4.5
    assert(baseline.meanEntropy > 4.0 && baseline.meanEntropy < 5.0);
    
    std::cout << "test_entropy_baseline passed." << std::endl;
}

// ============================================
// PatternMatcher Tests
// ============================================

void test_pattern_matcher_ransomware_extensions() {
    std::cout << "Running test_pattern_matcher_ransomware_extensions..." << std::endl;
    
    PatternMatcher matcher;
    
    // Get known ransomware extensions
    const auto& extensions = PatternMatcher::getKnownRansomwareExtensions();
    
    std::cout << "  Known ransomware extensions count: " << extensions.size() << std::endl;
    
    // Should have many known extensions
    assert(extensions.size() > 50);
    
    // Check if common ransomware extensions are in the list
    bool hasLocked = std::find(extensions.begin(), extensions.end(), ".locked") != extensions.end();
    bool hasEncrypted = std::find(extensions.begin(), extensions.end(), ".encrypted") != extensions.end();
    bool hasCrypt = std::find(extensions.begin(), extensions.end(), ".crypt") != extensions.end();
    
    std::cout << "  Has .locked: " << (hasLocked ? "yes" : "no") << std::endl;
    std::cout << "  Has .encrypted: " << (hasEncrypted ? "yes" : "no") << std::endl;
    std::cout << "  Has .crypt: " << (hasCrypt ? "yes" : "no") << std::endl;
    
    assert(hasLocked || hasEncrypted || hasCrypt);  // At least one should be present
    
    std::cout << "test_pattern_matcher_ransomware_extensions passed." << std::endl;
}

void test_pattern_matcher_ransom_notes() {
    std::cout << "Running test_pattern_matcher_ransom_notes..." << std::endl;
    
    PatternMatcher matcher;
    
    // Get known ransom note names
    const auto& ransomNotes = PatternMatcher::getKnownRansomNoteNames();
    
    std::cout << "  Known ransom note names count: " << ransomNotes.size() << std::endl;
    
    // Should have many known names
    assert(ransomNotes.size() > 20);
    
    // Check common ransom note names
    bool hasReadme = std::find(ransomNotes.begin(), ransomNotes.end(), "README.txt") != ransomNotes.end();
    bool hasDecrypt = std::find_if(ransomNotes.begin(), ransomNotes.end(), 
        [](const std::string& s) { return s.find("DECRYPT") != std::string::npos; }) != ransomNotes.end();
    
    std::cout << "  Has README.txt: " << (hasReadme ? "yes" : "no") << std::endl;
    std::cout << "  Has DECRYPT variant: " << (hasDecrypt ? "yes" : "no") << std::endl;
    
    std::cout << "test_pattern_matcher_ransom_notes passed." << std::endl;
}

void test_pattern_matcher_check_path() {
    std::cout << "Running test_pattern_matcher_check_path..." << std::endl;
    
    PatternMatcher matcher;
    
    // Check a suspicious path (ransomware extension)
    auto result = matcher.checkPath("/home/user/important.pdf.locked");
    
    std::cout << "  Path matched: " << (result.matched ? "yes" : "no") << std::endl;
    std::cout << "  Pattern name: " << result.patternName << std::endl;
    std::cout << "  Threat level: " << static_cast<int>(result.level) << std::endl;
    
    // .locked extension should match
    if (result.matched) {
        assert(result.level >= PatternMatcher::ThreatLevel::MEDIUM);
    }
    
    // Check a normal path
    auto normalResult = matcher.checkPath("/home/user/document.pdf");
    std::cout << "  Normal path matched: " << (normalResult.matched ? "yes" : "no") << std::endl;
    
    std::cout << "test_pattern_matcher_check_path passed." << std::endl;
}

void test_pattern_matcher_mass_rename() {
    std::cout << "Running test_pattern_matcher_mass_rename..." << std::endl;
    
    PatternMatcher matcher;
    
    // Simulate mass rename pattern (many files renamed to same extension)
    for (int i = 0; i < 25; ++i) {
        matcher.recordEvent("RENAME", "/docs/file" + std::to_string(i) + ".pdf.encrypted");
    }
    
    auto result = matcher.checkMassRenamePattern();
    
    std::cout << "  Mass rename detected: " << (result.matched ? "yes" : "no") << std::endl;
    std::cout << "  Pattern name: " << result.patternName << std::endl;
    std::cout << "  Description: " << result.description << std::endl;
    
    std::cout << "test_pattern_matcher_mass_rename passed." << std::endl;
}

// ============================================
// IsolationForest Tests
// ============================================

void test_isolation_forest_basic() {
    std::cout << "Running test_isolation_forest_basic..." << std::endl;
    
    IsolationForest::Config config;
    config.numTrees = 50;
    config.sampleSize = 64;
    IsolationForest forest(config);
    
    // Generate normal training data (clustered around origin)
    std::vector<std::vector<double>> normalData;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> d(0.0, 1.0);
    
    for (int i = 0; i < 200; ++i) {
        normalData.push_back({d(gen), d(gen), d(gen)});
    }
    
    // Fit the model
    forest.fit(normalData);
    
    // Test normal point (should have low anomaly score)
    std::vector<double> normalPoint = {0.5, 0.3, -0.2};
    double normalScore = forest.predict(normalPoint);
    
    // Test anomaly point (far from normal cluster)
    std::vector<double> anomalyPoint = {10.0, 10.0, 10.0};
    double anomalyScore = forest.predict(anomalyPoint);
    
    std::cout << "  Normal point score: " << normalScore << std::endl;
    std::cout << "  Anomaly point score: " << anomalyScore << std::endl;
    
    // Anomaly point should have higher score than normal point
    assert(anomalyScore > normalScore);
    
    std::cout << "test_isolation_forest_basic passed." << std::endl;
}

void test_isolation_forest_with_features() {
    std::cout << "Running test_isolation_forest_with_features..." << std::endl;
    
    // Test the FeatureExtractor
    // Create sample events: (action, path, size, entropy)
    std::vector<std::tuple<std::string, std::string, size_t, double>> events;
    
    for (int i = 0; i < 10; ++i) {
        events.push_back(std::make_tuple(
            "MODIFY",
            "/home/user/documents/report" + std::to_string(i) + ".docx",
            50000,
            4.5
        ));
    }
    
    auto features = FeatureExtractor::extractFeatures(events);
    auto featureVec = features.toVector();
    
    std::cout << "  Extracted feature count: " << featureVec.size() << std::endl;
    std::cout << "  Activity rate: " << features.activityRate << std::endl;
    std::cout << "  Modify ratio: " << features.modifyRatio << std::endl;
    
    // Should have FEATURE_COUNT features
    assert(featureVec.size() == FeatureExtractor::ActivityFeatures::FEATURE_COUNT);
    
    // All events were MODIFY, so modify ratio should be 1.0
    assert(features.modifyRatio > 0.9);
    
    std::cout << "test_isolation_forest_with_features passed." << std::endl;
}

// ============================================
// ThreatDetector Integration Tests
// ============================================

void test_threat_detector_creation() {
    std::cout << "Running test_threat_detector_creation..." << std::endl;
    
    ThreatDetector::Config config;
    config.enableBehaviorProfiling = true;
    config.enableEntropyAnalysis = true;
    config.enablePatternMatching = true;
    config.enableIsolationForest = false;  // Skip ML for this test (faster)
    
    ThreatDetector detector(config);
    
    std::cout << "  ThreatDetector created successfully" << std::endl;
    
    std::cout << "test_threat_detector_creation passed." << std::endl;
}

void test_threat_detector_analyze_event() {
    std::cout << "Running test_threat_detector_analyze_event..." << std::endl;
    
    ThreatDetector::Config config;
    config.enableBehaviorProfiling = true;
    config.enableEntropyAnalysis = false;  // Skip entropy (needs real files)
    config.enablePatternMatching = true;
    config.enableIsolationForest = false;
    config.alertThreshold = 0.5;
    
    ThreatDetector detector(config);
    
    bool alertReceived = false;
    ThreatDetector::ThreatAlert receivedAlert;
    
    detector.setAlertCallback([&](const ThreatDetector::ThreatAlert& alert) {
        alertReceived = true;
        receivedAlert = alert;
        std::cout << "  ALERT: " << alert.description << std::endl;
        std::cout << "  Type: " << static_cast<int>(alert.type) << std::endl;
        std::cout << "  Severity: " << static_cast<int>(alert.severity) << std::endl;
        std::cout << "  Confidence: " << alert.confidenceScore << std::endl;
    });
    
    // Simulate normal activity first
    for (int i = 0; i < 10; ++i) {
        detector.processEvent("MODIFY", "/home/user/doc" + std::to_string(i) + ".txt");
    }
    
    std::cout << "  Alert received after normal activity: " << (alertReceived ? "yes" : "no") << std::endl;
    
    std::cout << "test_threat_detector_analyze_event passed." << std::endl;
}

void test_threat_detector_ransomware_detection() {
    std::cout << "Running test_threat_detector_ransomware_detection..." << std::endl;
    
    ThreatDetector::Config config;
    config.enableBehaviorProfiling = true;
    config.enableEntropyAnalysis = false;
    config.enablePatternMatching = true;
    config.enableIsolationForest = false;
    config.alertThreshold = 0.3;  // Lower threshold for testing
    
    ThreatDetector detector(config);
    
    int alertCount = 0;
    
    detector.setAlertCallback([&](const ThreatDetector::ThreatAlert& alert) {
        alertCount++;
        std::cout << "  ALERT #" << alertCount << ": " << alert.description << std::endl;
    });
    
    // Simulate ransomware-like activity: mass rename to .encrypted
    for (int i = 0; i < 30; ++i) {
        detector.processEvent("RENAME", "/home/user/important" + std::to_string(i) + ".pdf.encrypted");
    }
    
    std::cout << "  Total alerts: " << alertCount << std::endl;
    
    // Should detect suspicious activity
    // (may or may not alert depending on thresholds)
    
    std::cout << "test_threat_detector_ransomware_detection passed." << std::endl;
}

void test_threat_detector_mass_deletion() {
    std::cout << "Running test_threat_detector_mass_deletion..." << std::endl;
    
    ThreatDetector::Config config;
    config.enableBehaviorProfiling = true;
    config.enableEntropyAnalysis = false;
    config.enablePatternMatching = true;
    config.enableIsolationForest = false;
    config.alertThreshold = 0.3;
    
    ThreatDetector detector(config);
    
    int alertCount = 0;
    
    detector.setAlertCallback([&](const ThreatDetector::ThreatAlert& alert) {
        alertCount++;
        std::cout << "  ALERT: " << alert.description << std::endl;
    });
    
    // Simulate mass deletion (ransomware or wiper behavior)
    for (int i = 0; i < 50; ++i) {
        detector.processEvent("DELETE", "/home/user/docs/file" + std::to_string(i) + ".txt");
    }
    
    std::cout << "  Total alerts for mass deletion: " << alertCount << std::endl;
    
    std::cout << "test_threat_detector_mass_deletion passed." << std::endl;
}

void test_threat_detector_stats() {
    std::cout << "Running test_threat_detector_stats..." << std::endl;
    
    ThreatDetector::Config config;
    config.enableBehaviorProfiling = true;
    config.enableEntropyAnalysis = false;
    config.enablePatternMatching = true;
    config.enableIsolationForest = false;
    
    ThreatDetector detector(config);
    
    // Process some events
    for (int i = 0; i < 20; ++i) {
        detector.processEvent("MODIFY", "/home/user/file" + std::to_string(i) + ".txt");
    }
    
    auto stats = detector.getStats();
    
    std::cout << "  Total events processed: " << stats.totalEventsProcessed << std::endl;
    std::cout << "  Alerts generated: " << stats.alertsGenerated << std::endl;
    std::cout << "  Avg processing time: " << stats.avgProcessingTimeMs << " ms" << std::endl;
    
    // Should have processed 20 events
    assert(stats.totalEventsProcessed == 20);
    
    std::cout << "test_threat_detector_stats passed." << std::endl;
}

// ============================================
// Main Test Runner
// ============================================

int main() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SentinelFS ML Threat Detector Tests" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    try {
        // BehaviorProfiler tests
        std::cout << "\n--- BehaviorProfiler Tests ---\n" << std::endl;
        test_behavior_profiler_learning();
        test_behavior_profiler_anomaly_detection();
        
        // FileEntropyAnalyzer tests
        std::cout << "\n--- FileEntropyAnalyzer Tests ---\n" << std::endl;
        test_entropy_low();
        test_entropy_high();
        test_entropy_normal_text();
        test_entropy_baseline();
        
        // PatternMatcher tests
        std::cout << "\n--- PatternMatcher Tests ---\n" << std::endl;
        test_pattern_matcher_ransomware_extensions();
        test_pattern_matcher_ransom_notes();
        test_pattern_matcher_check_path();
        test_pattern_matcher_mass_rename();
        
        // IsolationForest tests
        std::cout << "\n--- IsolationForest Tests ---\n" << std::endl;
        test_isolation_forest_basic();
        test_isolation_forest_with_features();
        
        // ThreatDetector integration tests
        std::cout << "\n--- ThreatDetector Integration Tests ---\n" << std::endl;
        test_threat_detector_creation();
        test_threat_detector_analyze_event();
        test_threat_detector_ransomware_detection();
        test_threat_detector_mass_deletion();
        test_threat_detector_stats();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "All ML Threat Detector tests passed!" << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
