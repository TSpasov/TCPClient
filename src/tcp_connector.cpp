#include "tcp_connector.hpp"
#include <stdexcept>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>

TCPConnector::TCPConnector(const std::string& ip, int port) {
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) throw std::runtime_error("socket() failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ::inet_addr(ip.c_str());

    if (::connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ::close(sockfd);
        throw std::runtime_error("connect() failed");
    }

    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

TCPConnector::~TCPConnector() {
    if (sockfd >= 0) ::close(sockfd);
}

void TCPConnector::send(const std::string& msg) {
    if (::write(sockfd, msg.c_str(), msg.size()) < 0) {
        throw std::runtime_error("send() failed");
    }
}

std::string TCPConnector::read(int timeout_ms) {
    pollfd fds{};
    fds.fd = sockfd;
    fds.events = POLLIN;

    int poll_count = ::poll(&fds, 1, timeout_ms);
    if (poll_count < 0) throw std::runtime_error("poll() failed");
    if (poll_count == 0) throw std::runtime_error("read timeout");

    if (fds.revents & POLLIN) {
        char buf[1024];
        ssize_t n = ::read(sockfd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            if (n == 0) throw std::runtime_error("server closed connection");
            throw std::runtime_error("read() failed");
        }
        buf[n] = '\0';
        return std::string(buf);
    }
    return {};
}

