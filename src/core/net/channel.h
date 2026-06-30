#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

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
    // 超时返回 false（payload 不变）。
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

    // TCP 为阻塞读，忽略 timeoutMs。
    bool recvMessage(net::MsgType& type, std::vector<uint8_t>& payload,
                     int /*timeoutMs*/) override {
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
