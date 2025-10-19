#pragma once

#include <string>
#include <map>

struct Config {
    std::string sessionCode;
    std::string syncPath;
    int port;
    bool verbose;
    bool daemonMode;
    int discoveryInterval;
    int remeshThreshold;
    std::string configFile;
    
    Config() : port(8080), verbose(false), daemonMode(false), 
               discoveryInterval(5000), remeshThreshold(100) {}
};

class CLI {
public:
    CLI();
    ~CLI();
    
    Config parseArguments(int argc, char* argv[]);
    void printUsage();
    void printVersion();
    
private:
    Config defaultConfig;
};