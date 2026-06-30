#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/capture/screen_capture.h"
#include "core/codec/h264_encoder.h"
#include "core/input/input_event.h"
#include "core/input/input_injector.h"
#include "core/net/channel.h"
#include "core/net/tcp_socket.h"
#include "device_id_gen.h"

namespace rd {

// Headless 服务：无 GUI 的纯后台服务模式。
// 适合部署在 Linux/Windows 服务器上，不依赖 Qt GUI 模块。
// 只做 host 功能（被控端）：监听端口，接受连接，推流 + 接收输入。
class HeadlessService {
public:
    HeadlessService();
    ~HeadlessService();

    void setPassword(const std::string& pwd) { password_ = pwd; }

    // 阻塞运行（主循环），返回退出码
    int run(uint16_t port = 9000);

    // 请求停止
    void requestStop() { running_.store(false); }

private:
    void handleClient(rd::net::TcpConn conn);
    bool authenticateClient(IChannel& ch);

    std::string password_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> clientThreads_;
    std::mutex threadsMtx_;

    DeviceIdentity identity_;
};

}  // namespace rd
