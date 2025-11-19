#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

const char* SOCKET_PATH = "/tmp/sentinel_daemon.sock";

bool sendCommand(const std::string& command, std::string& response) {
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error: Cannot create socket" << std::endl;
        return false;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error: Cannot connect to daemon. Is it running?" << std::endl;
        close(sockfd);
        return false;
    }

    // Send command
    send(sockfd, command.c_str(), command.length(), 0);

    // Receive response
    char buffer[4096];
    ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        response = buffer;
    }

    close(sockfd);
    return true;
}

void printUsage(const char* progName) {
    std::cout << "SentinelFS CLI - Control Interface\n";
    std::cout << "\nUsage: " << progName << " [command] [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  status              Show daemon status and sync information\n";
    std::cout << "  peers               List all discovered peers\n";
    std::cout << "  logs [n]            Show last n log entries (default: 20)\n";
    std::cout << "  config              Display current configuration\n";
    std::cout << "  pause               Pause file synchronization\n";
    std::cout << "  resume              Resume file synchronization\n";
    std::cout << "  stats               Show transfer statistics\n";
    std::cout << "  help                Show this help message\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << progName << " status\n";
    std::cout << "  " << progName << " peers\n";
    std::cout << "  " << progName << " logs 50\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string command = argv[1];
    std::string fullCommand;

    if (command == "help" || command == "--help" || command == "-h") {
        printUsage(argv[0]);
        return 0;
    } else if (command == "status") {
        fullCommand = "STATUS";
    } else if (command == "peers") {
        fullCommand = "PEERS";
    } else if (command == "logs") {
        int n = 20;
        if (argc > 2) {
            n = std::stoi(argv[2]);
        }
        fullCommand = "LOGS|" + std::to_string(n);
    } else if (command == "config") {
        fullCommand = "CONFIG";
    } else if (command == "pause") {
        fullCommand = "PAUSE";
    } else if (command == "resume") {
        fullCommand = "RESUME";
    } else if (command == "stats") {
        fullCommand = "STATS";
    } else {
        std::cerr << "Error: Unknown command '" << command << "'\n";
        std::cerr << "Run '" << argv[0] << " help' for usage information.\n";
        return 1;
    }

    std::string response;
    if (sendCommand(fullCommand, response)) {
        std::cout << response;
        if (!response.empty() && response.back() != '\n') {
            std::cout << std::endl;
        }
        return 0;
    } else {
        return 1;
    }
}