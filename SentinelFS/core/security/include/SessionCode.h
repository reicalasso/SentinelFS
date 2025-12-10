#pragma once

#include <string>
#include <random>
#include <sstream>
#include <iomanip>

namespace SentinelFS {

    class SessionCode {
    public:
        // Generate a random 6-character alphanumeric session code
        static std::string generate() {
            const char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // Exclude similar looking chars
            const int length = 6;
            
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
            
            std::string code;
            for (int i = 0; i < length; ++i) {
                code += charset[dis(gen)];
            }
            
            return code;
        }
        
        // Validate session code format (6 alphanumeric characters)
        static bool isValid(const std::string& code) {
            if (code.length() != 6) return false;
            
            for (char c : code) {
                if (!std::isalnum(c)) return false;
            }
            
            return true;
        }
        
        // Format session code with dashes for display (e.g., ABC-DEF)
        static std::string format(const std::string& code) {
            if (code.length() != 6) return code;
            return code.substr(0, 3) + "-" + code.substr(3, 3);
        }
        
        // Remove dashes from formatted code
        static std::string normalize(const std::string& code) {
            std::string normalized;
            for (char c : code) {
                if (c != '-' && c != ' ') {
                    normalized += std::toupper(c);
                }
            }
            return normalized;
        }
    };

}
