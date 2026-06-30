#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

#include "core/net/tcp_socket.h"

namespace rd {

// 信令客户端：连接 signal_server，完成配对，并在配对后透传文本消息
// （用于交换 WebRTC 的 SDP 描述与 ICE 候选）。
class SignalClient {
public:
    using PairedCb = std::function<void()>;
    using RelayCb = std::function<void(const std::string&)>;
    using ErrorCb = std::function<void(const std::string&)>;

    ~SignalClient();

    bool connect(const std::string& host, uint16_t port);

    // host 端：登记 id，等待对端
    bool sendRegister(const std::string& id);
    // viewer 端：请求连接某个 id
    bool sendConnect(const std::string& id);
    // 透传一段文本给已配对的对端
    bool sendRelay(const std::string& text);

    // 启动后台接收线程。回调在该线程触发。
    void start(PairedCb onPaired, RelayCb onRelay, ErrorCb onError);
    void stop();

private:
    net::TcpConn conn_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

}  // namespace rd
