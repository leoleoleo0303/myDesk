#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <cerrno>
#endif

#include "core/net/message.h"
#include "core/net/tcp_socket.h"

namespace rd {

// 传输无关的消息通道接口。host/viewer 的业务逻辑面向它编程，
// 底层既可以是局域网 TCP，也可以是 P2P 的 DataChannel。
class IChannel {
public:
    virtual ~IChannel() = default;

    // 发送一条消息（type + payload）。
    virtual bool sendMessage(net::MsgType type, const void* data,
                             uint32_t len) = 0;

    // 接收一条消息。timeoutMs<0 表示阻塞直到有消息或断开；
    // timeoutMs>0 时先用 select 等待可读，超时返回 false（payload 不变）。
    virtual bool recvMessage(net::MsgType& type, std::vector<uint8_t>& payload,
                             int timeoutMs) = 0;

    // 通道是否仍然连通（用于区分"超时"与"对端断开"）。
    virtual bool isConnected() const = 0;
};

// 局域网 TCP 通道。底层是一条 TcpConn。
class TcpChannel : public IChannel {
public:
    explicit TcpChannel(net::TcpConn conn) : conn_(std::move(conn)) {}

    bool sendMessage(net::MsgType type, const void* data,
                     uint32_t len) override {
        std::lock_guard<std::mutex> lk(sendMtx_);
        if (!net::sendMessage(conn_, type, data, len)) {
            open_.store(false);
            return false;
        }
        return true;
    }

    bool recvMessage(net::MsgType& type, std::vector<uint8_t>& payload,
                     int timeoutMs) override {
        // 使用 select 实现超时等待，避免 SO_RCVTIMEO 在 recvAll 中间
        // 截断导致帧错乱。select 返回可读后再进行完整阻塞读。
        if (timeoutMs > 0) {
            auto sock = static_cast<SOCKET>(conn_.handle());
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);

            struct timeval tv;
            tv.tv_sec = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;

            int ret = ::select(static_cast<int>(sock) + 1, &readfds,
                               nullptr, nullptr, &tv);
            if (ret <= 0) {
                // ret == 0: timeout; ret < 0: error
                if (ret < 0) open_.store(false);
                return false;
            }
            // 数据已就绪，继续阻塞读取完整消息
        }

        if (!net::recvMessage(conn_, type, payload)) {
            open_.store(false);
            return false;
        }
        return true;
    }

    bool isConnected() const override { return open_.load(); }

    net::TcpConn& conn() { return conn_; }

private:
    net::TcpConn conn_;
    std::mutex sendMtx_;
    std::atomic<bool> open_{true};
};

}  // namespace rd
