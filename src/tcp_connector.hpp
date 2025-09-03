#pragma once
#include <string>

class TCPConnector {
    int sockfd{-1};
    std::string ip;
    int port;
    bool handshaked{false};

    void open_connection();      // establish TCP connection
    void do_handshake();         // send HELLO, expect HELLO
    void close_connection();     // cleanup

public:
    TCPConnector(const std::string& ip, int port);
    ~TCPConnector();

    void send(const std::string& msg);
    std::string read(int timeout_ms = 3000); // returns one line
};
