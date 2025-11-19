#include "DeltaSyncProtocolHandler.h"
#include "DeltaSerialization.h"
#include "DeltaEngine.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include "IFileAPI.h"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace SentinelFS {

DeltaSyncProtocolHandler::DeltaSyncProtocolHandler(INetworkAPI* network, IStorageAPI* storage,
                                                   IFileAPI* filesystem, const std::string& watchDir)
    : network_(network), storage_(storage), filesystem_(filesystem), watchDirectory_(watchDir) {}

void DeltaSyncProtocolHandler::handleUpdateAvailable(const std::string& peerId, 
                                                     const std::vector<uint8_t>& rawData) {
    std::string fullMsg(rawData.begin(), rawData.end());
    std::string filename = fullMsg.substr(17); // Skip "UPDATE_AVAILABLE|"
    
    std::cout << "Peer " << peerId << " has update for: " << filename << std::endl;
    
    std::string localPath = watchDirectory_ + "/" + filename;
    
    // Calculate local signature
    std::vector<BlockSignature> sigs;
    if (std::filesystem::exists(localPath)) {
        sigs = DeltaEngine::calculateSignature(localPath);
    }
    
    auto serializedSig = DeltaSerialization::serializeSignature(sigs);
    
    // Send delta request
    std::string header = "REQUEST_DELTA|" + filename + "|";
    std::vector<uint8_t> payload(header.begin(), header.end());
    payload.insert(payload.end(), serializedSig.begin(), serializedSig.end());
    
    network_->sendData(peerId, payload);
}

void DeltaSyncProtocolHandler::handleDeltaRequest(const std::string& peerId,
                                                  const std::vector<uint8_t>& rawData) {
    std::string msg(rawData.begin(), rawData.end());
    
    size_t firstPipe = msg.find('|');
    size_t secondPipe = msg.find('|', firstPipe + 1);
    
    if (secondPipe != std::string::npos) {
        std::string filename = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        
        if (rawData.size() > secondPipe + 1) {
            std::vector<uint8_t> sigData(rawData.begin() + secondPipe + 1, rawData.end());
            auto sigs = DeltaSerialization::deserializeSignature(sigData);
            
            std::cout << "Received delta request for: " << filename << " from " << peerId << std::endl;
            
            std::string localPath = watchDirectory_ + "/" + filename;
            if (std::filesystem::exists(localPath)) {
                auto deltas = DeltaEngine::calculateDelta(localPath, sigs);
                auto serializedDelta = DeltaSerialization::serializeDelta(deltas);
                
                std::string header = "DELTA_DATA|" + filename + "|";
                std::vector<uint8_t> payload(header.begin(), header.end());
                payload.insert(payload.end(), serializedDelta.begin(), serializedDelta.end());
                
                network_->sendData(peerId, payload);
                std::cout << "Sent delta with " << deltas.size() << " instructions" << std::endl;
            }
        }
    }
}

void DeltaSyncProtocolHandler::handleDeltaData(const std::string& peerId,
                                               const std::vector<uint8_t>& rawData) {
    std::string msg(rawData.begin(), rawData.end());
    
    size_t firstPipe = msg.find('|');
    size_t secondPipe = msg.find('|', firstPipe + 1);
    
    if (secondPipe != std::string::npos) {
        std::string filename = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        
        if (rawData.size() > secondPipe + 1) {
            std::vector<uint8_t> deltaData(rawData.begin() + secondPipe + 1, rawData.end());
            auto deltas = DeltaSerialization::deserializeDelta(deltaData);
            
            std::cout << "Received delta data for: " << filename << " from " << peerId << std::endl;
            
            std::string localPath = watchDirectory_ + "/" + filename;
            
            // Create file if doesn't exist
            if (!std::filesystem::exists(localPath)) {
                std::ofstream outfile(localPath);
                outfile.close();
            }
            
            // Apply delta
            auto newData = DeltaEngine::applyDelta(localPath, deltas);
            
            // Mark as patched to prevent sync loop
            if (markAsPatchedCallback_) {
                markAsPatchedCallback_(filename);
            }
            
            filesystem_->writeFile(localPath, newData);
            std::cout << "Patched file: " << localPath << std::endl;
        }
    }
}

} // namespace SentinelFS
