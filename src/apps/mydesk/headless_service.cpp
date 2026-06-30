#include "headless_service.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>

namespace rd {

namespace {
constexpr uint8_t kMsgAuth = 20;
constexpr uint8_t kMsgAuthOk = 21;
constexpr uint8_t kMsgAuthFail = 22;

std::atomic<bool>* g_runningPtr = nullptr;

void signalHandler(int /*sig*/) {
    if (g_runningPtr) g_runningPtr->store(false);
}
}  // namespace

HeadlessService::HeadlessService() {
    identity_ = DeviceIdentity::load();
}

HeadlessService::~HeadlessService() {
    running_.store(false);
    std::lock_guard<std::mutex> lk(threadsMtx_);
    for (auto& t : clientThreads_) {
        if (t.joinable()) t.join();
    }
}

int HeadlessService::run(uint16_t port) {
    running_.store(true);
    g_runningPtr = &running_;

    // 注册信号处理，支持 Ctrl+C 优雅退出
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 如果没有设置密码，使用生成的密码
    if (password_.empty()) {
        password_ = identity_.password;
    }

    std::printf("=========================================\n");
    std::printf("  myDesk Headless Service\n");
    std::printf("=========================================\n");
    std::printf("  Device ID: %s\n", identity_.deviceId.c_str());
    std::printf("  Password:  %s\n", password_.c_str());
    std::printf("  Port:      %u\n", port);
    std::printf("  Press Ctrl+C to stop\n");
    std::printf("=========================================\n");

    net::TcpListener listener;
    if (!listener.listen(port)) {
        std::fprintf(stderr, "ERROR: Failed to listen on port %u\n", port);
        return 1;
    }

    while (running_.load()) {
        net::TcpConn conn = listener.accept();
        if (!conn.valid()) {
            if (running_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }

        std::printf("[headless] New client connected\n");

        std::lock_guard<std::mutex> lk(threadsMtx_);
        clientThreads_.emplace_back(
            [this, c = std::move(conn)]() mutable { handleClient(std::move(c)); });
    }

    // 等待客户端线程结束
    {
        std::lock_guard<std::mutex> lk(threadsMtx_);
        for (auto& t : clientThreads_) {
            if (t.joinable()) t.join();
        }
        clientThreads_.clear();
    }

    std::printf("[headless] Service stopped.\n");
    return 0;
}

bool HeadlessService::authenticateClient(IChannel& ch) {
    if (password_.empty()) return true;

    net::MsgType type;
    std::vector<uint8_t> payload;
    if (!ch.recvMessage(type, payload, 10000)) return false;
    if (static_cast<uint8_t>(type) != kMsgAuth) return false;

    const std::string clientPwd(payload.begin(), payload.end());
    if (clientPwd == password_) {
        ch.sendMessage(static_cast<net::MsgType>(kMsgAuthOk), nullptr, 0);
        return true;
    } else {
        const char* msg = "wrong password";
        ch.sendMessage(static_cast<net::MsgType>(kMsgAuthFail), msg,
                       static_cast<uint32_t>(std::strlen(msg)));
        return false;
    }
}

void HeadlessService::handleClient(net::TcpConn conn) {
    TcpChannel ch(std::move(conn));

    if (!authenticateClient(ch)) {
        std::printf("[headless] Client auth failed, disconnected\n");
        return;
    }
    std::printf("[headless] Client authenticated, starting stream\n");

    auto cap = ScreenCapture::create();
    if (!cap || !cap->init()) {
        std::fprintf(stderr, "[headless] Screen capture init failed\n");
        return;
    }

    Frame frame;
    if (!cap->captureFrame(frame) || frame.empty()) {
        std::fprintf(stderr, "[headless] First frame capture failed\n");
        return;
    }

    H264Encoder enc;
    H264Encoder::Config cfg;
    cfg.width = frame.width;
    cfg.height = frame.height;
    cfg.fps = 30;
    cfg.bitrateKbps = 8000;
    cfg.gopSize = 60;
    if (!enc.open(cfg)) {
        std::fprintf(stderr, "[headless] Encoder open failed\n");
        return;
    }

    auto injector = InputInjector::create();
    if (injector) {
        injector->init(frame.width, frame.height);
    }

    std::atomic<bool> sessionRunning{true};
    std::thread inputThread([&]() {
        net::MsgType type;
        std::vector<uint8_t> payload;
        while (sessionRunning.load() && running_.load()) {
            if (!ch.recvMessage(type, payload, 100)) {
                if (!ch.isConnected()) break;
                continue;
            }
            if (type == net::MsgType::Input && injector) {
                InputEvent ev;
                if (deserializeInput(payload.data(), payload.size(), ev)) {
                    injector->inject(ev);
                }
            }
        }
    });

    const auto interval = std::chrono::milliseconds(1000 / 30);
    std::vector<EncodedPacket> packets;

    while (running_.load() && sessionRunning.load()) {
        const auto t0 = std::chrono::steady_clock::now();

        if (!cap->captureFrame(frame) || frame.empty()) break;

        packets.clear();
        if (!enc.encode(frame, packets)) break;

        bool peerAlive = true;
        for (const auto& p : packets) {
            if (!ch.sendMessage(net::MsgType::Video, p.data.data(),
                                static_cast<uint32_t>(p.data.size()))) {
                peerAlive = false;
                break;
            }
        }
        if (!peerAlive) break;

        const auto elapsed = std::chrono::steady_clock::now() - t0;
        if (elapsed < interval)
            std::this_thread::sleep_for(interval - elapsed);
    }

    sessionRunning.store(false);
    if (inputThread.joinable()) inputThread.join();
    std::printf("[headless] Client session ended\n");
}

}  // namespace rd
