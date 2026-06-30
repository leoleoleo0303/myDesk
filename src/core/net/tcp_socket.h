#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace rd::net {

#if defined(_WIN32)
using SocketHandle = unsigned long long;  // Windows SOCKET
#else
using SocketHandle = int;
#endif

// 进程启动/退出时调用一次（Windows 需要初始化 Winsock；Linux 为空操作）。
bool initSockets();
void shutdownSockets();

// 一条已建立的 TCP 连接，move-only。
class TcpConn {
public:
    TcpConn() = default;
    explicit TcpConn(SocketHandle fd) : fd_(fd) {}
    ~TcpConn();

    TcpConn(TcpConn&& o) noexcept;
    TcpConn& operator=(TcpConn&& o) noexcept;
    TcpConn(const TcpConn&) = delete;
    TcpConn& operator=(const TcpConn&) = delete;

    bool valid() const;
    void close();

    // 获取底层 socket handle（用于设置 socket 选项）
    SocketHandle handle() const { return fd_; }

    // 完整发送 len 字节，失败（含对端断开）返回 false。
    bool sendAll(const void* data, size_t len);
    // 完整接收 len 字节，对端关闭或出错返回 false。
    bool recvAll(void* data, size_t len);

    // 连接到 host:port，失败返回无效连接。
    static TcpConn connect(const std::string& host, uint16_t port);

private:
    SocketHandle fd_ = static_cast<SocketHandle>(-1);
};

// 监听并接受连接。
class TcpListener {
public:
    ~TcpListener();

    bool listen(uint16_t port);
    TcpConn accept();  // 阻塞等待一个连接
    void close();

private:
    SocketHandle fd_ = static_cast<SocketHandle>(-1);
};

}  // namespace rd::net
