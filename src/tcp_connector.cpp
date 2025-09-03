#include "tcp_connector.hpp"
#include <stdexcept>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <chrono>
#include <thread>

namespace {
    // ---- Protocol constants ----
    constexpr int CONNECT_TIMEOUT_MS = 2000;   // max connect attempt
    constexpr int READ_TIMEOUT_MS    = 3000;   // idle/read timeout
    constexpr int RETRY_MAX          = 2;      // extra retries (total = 3 attempts)
    constexpr int BACKOFF1_MS        = 200;
    constexpr int BACKOFF2_MS        = 600;

    const std::string HELLO_CLI  = "HELLO|GW|1.0\n";
    const std::string HELLO_TERM = "HELLO|TERM|1.0";
    const std::string PING       = "PING\n";
    const std::string PONG       = "PONG";

    int connect_with_timeout(int sockfd, sockaddr_in& addr, int timeout_ms) {
        // make non-blocking
        int flags = fcntl(sockfd, F_GETFL, 0);
        fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

        int res = ::connect(sockfd, (sockaddr*)&addr, sizeof(addr));
        if (res == 0) return 0;
        if (errno != EINPROGRESS) return -1;

        pollfd pfd{};
        pfd.fd = sockfd;
        pfd.events = POLLOUT;

        int ret = poll(&pfd, 1, timeout_ms);
        if (ret > 0 && (pfd.revents & POLLOUT)) {
            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len);
            return err ? -1 : 0;
        }
        return -1;
    }
} // anonymous namespace

// ---- TCPConnector impl ----

void TCPConnector::open_connection() {
    sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) throw std::runtime_error("socket() failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    addr.sin_addr.s_addr = ::inet_addr(ip.c_str());

    // retry loop: initial + RETRY_MAX
    for (int attempt = 0; attempt <= RETRY_MAX; ++attempt) {
        if (connect_with_timeout(sockfd, addr, CONNECT_TIMEOUT_MS) == 0) return;

        ::close(sockfd);
        sockfd = -1;

        if (attempt < RETRY_MAX) {
            int backoff = (attempt == 0) ? BACKOFF1_MS : BACKOFF2_MS;
            std::this_thread::sleep_for(std::chrono::milliseconds(backoff));
            sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) throw std::runtime_error("socket() failed");
        }
    }
    throw std::runtime_error("connect() failed after retries");
}

void TCPConnector::do_handshake() {
    if (::write(sockfd, HELLO_CLI.c_str(), HELLO_CLI.size()) < 0) {
        throw std::runtime_error("handshake send failed");
    }

    pollfd fds{};
    fds.fd = sockfd;
    fds.events = POLLIN;

    int ret = poll(&fds, 1, READ_TIMEOUT_MS);
    if (ret <= 0) throw std::runtime_error("handshake timeout");

    char buf[128];
    ssize_t n = ::read(sockfd, buf, sizeof(buf) - 1);
    if (n <= 0) throw std::runtime_error("handshake failed");
    buf[n] = '\0';
    std::string resp(buf);

    if (resp.find(HELLO_TERM) == std::string::npos) {
        throw std::runtime_error("invalid handshake response: " + resp);
    }
    handshaked = true;
}

TCPConnector::TCPConnector(const std::string& ip_, int port_)
    : ip(ip_), port(port_) {
    open_connection();
    try {
        do_handshake();
    } catch (...) {
        // reconnect once if server drops right after HELLO
        close_connection();
        open_connection();
        do_handshake();
    }
}

TCPConnector::~TCPConnector() {
    close_connection();
}

void TCPConnector::close_connection() {
    if (sockfd >= 0) {
        ::close(sockfd);
        sockfd = -1;
    }
}

void TCPConnector::send(const std::string& msg) {
    std::string line = msg;
    if (line.empty() || line.back() != '\n') line.push_back('\n');
    if (::write(sockfd, line.c_str(), line.size()) < 0) {
        throw std::runtime_error("send() failed");
    }
}

std::string TCPConnector::read(int timeout_ms) {
    pollfd fds{};
    fds.fd = sockfd;
    fds.events = POLLIN;

    int ret = poll(&fds, 1, timeout_ms);
    if (ret == 0) {
        // idle → send keepalive
        ::write(sockfd, PING.c_str(), PING.size());

        // wait again
        ret = poll(&fds, 1, timeout_ms);
        if (ret <= 0) throw std::runtime_error("keepalive timeout");
    }
    if (ret < 0) throw std::runtime_error("poll() failed");

    char buf[1024];
    ssize_t n = ::read(sockfd, buf, sizeof(buf) - 1);
    if (n <= 0) throw std::runtime_error("read() failed");
    buf[n] = '\0';

    std::string resp(buf);
    if (resp.find(PONG) != std::string::npos) {
        // swallow keepalive and recurse to get actual data
        return read(timeout_ms);
    }
    return resp;
}
