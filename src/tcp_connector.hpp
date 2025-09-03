#pragma once
#include <string>

class TCPConnector {
    int sockfd{-1};

public:
    TCPConnector(const std::string& ip, int port);
    ~TCPConnector();

    void send(const std::string& msg);
    std::string read(int timeout_ms = 5000);
};

