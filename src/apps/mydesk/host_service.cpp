#include "host_service.h"

#include <chrono>
#include <cstdio>
#include <cstring>

#include "core/net/tcp_socket.h"

#if defined(RD_HAVE_P2P)
#include "core/p2p/rtc_transport.h"
#endif

namespace rd {

namespace {
constexpr uint8_t kMsgAuth = 20;
constexpr uint8_t kMsgAuthOk = 21;
constexpr uint8_t kMsgAuthFail = 22;
}  // namespace

HostService::HostService(QObject* parent) : QObject(parent) {}

HostService::~HostService() { stop(); }

bool HostService::startListening(uint16_t port) {
    if (running_.load()) return false;
    port_ = port;
    running_.store(true);
    listenThread_ = std::thread([this]() { listenLoop(); });
    return true;
}

bool HostService::registerP2P(const std::string& signalHost,
                              uint16_t signalPort,
                              const std::string& deviceId) {
#if defined(RD_HAVE_P2P)
    if (!running_.load()) {
        running_.store(true);
    }
    p2pThread_ = std::thread([this, signalHost, signalPort, deviceId]() {
        p2pLoop(signalHost, signalPort, deviceId);
    });
    return true;
#else
    emit p2pRegisterFailed("P2P not enabled (build without RD_ENABLE_P2P)");
    return false;
#endif
}

void HostService::stop() {
    running_.store(false);
    p2pRegistered_.store(false);
    if (listenThread_.joinable()) listenThread_.join();
    if (p2pThread_.joinable()) p2pThread_.join();
    std::lock_guard<std::mutex> lk(threadsMtx_);
    for (auto& t : clientThreads_) {
        if (t.joinable()) t.join();
    }
    clientThreads_.clear();
}

void HostService::listenLoop() {
    net::TcpListener listener;
    if (!listener.listen(port_)) {
        emit error(QString("Listen on port %1 failed").arg(port_));
        running_.store(false);
        return;
    }

    while (running_.load()) {
        net::TcpConn conn = listener.accept();
        if (!conn.valid()) {
            if (running_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }

        auto ch = std::make_unique<TcpChannel>(std::move(conn));

        std::lock_guard<std::mutex> lk(threadsMtx_);
        clientThreads_.emplace_back(
            [this, ch = std::move(ch)]() mutable { handleClient(std::move(ch)); });
    }
}

void HostService::p2pLoop(std::string signalHost, uint16_t signalPort,
                          std::string deviceId) {
#if defined(RD_HAVE_P2P)
    // Continuously register with signal_server.
    // When a viewer connects, handle the session, then re-register for the next one.
    while (running_.load()) {
        auto rtc = std::make_unique<RtcTransport>();
        if (!rtc->startHost(signalHost, signalPort, deviceId)) {
            emit p2pRegisterFailed("Cannot connect to signal server");
            // Retry after a delay
            for (int i = 0; i < 50 && running_.load(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        p2pRegistered_.store(true);
        emit p2pRegistered();
        std::printf("[host] P2P registered with id=%s, waiting for viewer...\n",
                    deviceId.c_str());

        // Wait for a viewer to connect (blocking)
        if (!rtc->waitConnected(-1)) {
            // Interrupted (stop() called or signal_server disconnected)
            p2pRegistered_.store(false);
            if (!running_.load()) break;
            // Retry
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        p2pRegistered_.store(false);
        // Handle the P2P client session
        handleClient(std::move(rtc));

        // After session ends, loop back and re-register for next viewer
        if (!running_.load()) break;
        std::printf("[host] P2P session ended, re-registering...\n");
    }
    p2pRegistered_.store(false);
#endif
}

bool HostService::authenticateClient(IChannel& ch) {
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

void HostService::handleClient(std::unique_ptr<IChannel> ch) {
    clientCount_.fetch_add(1);
    emit clientConnected("new client");

    if (!authenticateClient(*ch)) {
        clientCount_.fetch_sub(1);
        emit clientDisconnected();
        return;
    }

    auto cap = ScreenCapture::create();
    if (!cap || !cap->init()) {
        clientCount_.fetch_sub(1);
        emit clientDisconnected();
        return;
    }

    Frame frame;
    if (!cap->captureFrame(frame) || frame.empty()) {
        clientCount_.fetch_sub(1);
        emit clientDisconnected();
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
        clientCount_.fetch_sub(1);
        emit clientDisconnected();
        return;
    }

    auto injector = InputInjector::create();
    if (injector) injector->init(frame.width, frame.height);

    std::atomic<bool> sessionRunning{true};
    std::thread inputThread([&]() {
        net::MsgType type;
        std::vector<uint8_t> payload;
        while (sessionRunning.load()) {
            if (!ch->recvMessage(type, payload, 100)) {
                if (!ch->isConnected()) break;
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
            if (!ch->sendMessage(net::MsgType::Video, p.data.data(),
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

    clientCount_.fetch_sub(1);
    emit clientDisconnected();
}

}  // namespace rd
