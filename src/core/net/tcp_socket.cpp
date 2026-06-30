#include "core/net/tcp_socket.h"

#include <cstring>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_t = int;
static int closeSock(rd::net::SocketHandle s) { return ::closesocket(s); }
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
static int closeSock(rd::net::SocketHandle s) { return ::close(s); }
#endif

namespace rd::net {

namespace {
constexpr SocketHandle kInvalid = static_cast<SocketHandle>(-1);

void setNoDelay(SocketHandle fd) {
    int one = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                 reinterpret_cast<const char*>(&one), sizeof(one));
}
}  // namespace

bool initSockets() {
#if defined(_WIN32)
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
#else
    return true;
#endif
}

void shutdownSockets() {
#if defined(_WIN32)
    WSACleanup();
#endif
}

// ---- TcpConn ----

TcpConn::~TcpConn() { close(); }

TcpConn::TcpConn(TcpConn&& o) noexcept : fd_(o.fd_) { o.fd_ = kInvalid; }

TcpConn& TcpConn::operator=(TcpConn&& o) noexcept {
    if (this != &o) {
        close();
        fd_ = o.fd_;
        o.fd_ = kInvalid;
    }
    return *this;
}

bool TcpConn::valid() const { return fd_ != kInvalid; }

void TcpConn::close() {
    if (fd_ != kInvalid) {
        closeSock(fd_);
        fd_ = kInvalid;
    }
}

bool TcpConn::sendAll(const void* data, size_t len) {
    if (fd_ == kInvalid) return false;
    const char* p = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < len) {
        const auto n = ::send(fd_, p + sent, static_cast<int>(len - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool TcpConn::recvAll(void* data, size_t len) {
    if (fd_ == kInvalid) return false;
    char* p = static_cast<char*>(data);
    size_t got = 0;
    while (got < len) {
        const auto n = ::recv(fd_, p + got, static_cast<int>(len - got), 0);
        if (n <= 0) return false;  // 0 = 对端关闭，<0 = 错误
        got += static_cast<size_t>(n);
    }
    return true;
}

TcpConn TcpConn::connect(const std::string& host, uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0) {
        return TcpConn{};
    }

    SocketHandle fd = kInvalid;
    for (addrinfo* ai = res; ai; ai = ai->ai_next) {
        fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == kInvalid) continue;
        if (::connect(fd, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen)) ==
            0) {
            break;
        }
        closeSock(fd);
        fd = kInvalid;
    }
    ::freeaddrinfo(res);

    if (fd != kInvalid) setNoDelay(fd);
    return TcpConn{fd};
}

// ---- TcpListener ----

TcpListener::~TcpListener() { close(); }

bool TcpListener::listen(uint16_t port) {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == kInvalid) return false;

    int one = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&one), sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close();
        return false;
    }
    if (::listen(fd_, 1) != 0) {
        close();
        return false;
    }
    return true;
}

TcpConn TcpListener::accept() {
    if (fd_ == kInvalid) return TcpConn{};
    SocketHandle c = ::accept(fd_, nullptr, nullptr);
    if (c != kInvalid) setNoDelay(c);
    return TcpConn{c};
}

void TcpListener::close() {
    if (fd_ != kInvalid) {
        closeSock(fd_);
        fd_ = kInvalid;
    }
}

}  // namespace rd::net
