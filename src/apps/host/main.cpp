// host（被控端）：采集 -> 编码 -> 发送；同时接收对端输入并注入。
//
// 两种模式：
//   局域网:  host [端口] [最大帧数]                监听端口等 viewer 直连
//   公网P2P: host --p2p <信令服务器IP> <端口> <id>  经信令服务器配对 + 打洞
//
// 默认端口 9000；P2P 默认信令端口需显式给出。

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/capture/screen_capture.h"
#include "core/codec/h264_encoder.h"
#include "core/input/input_event.h"
#include "core/input/input_injector.h"
#include "core/net/channel.h"
#include "core/net/message.h"
#include "core/net/tcp_socket.h"

#if defined(RD_HAVE_P2P)
#include "core/p2p/rtc_transport.h"
#endif

namespace {

std::atomic<bool> g_running{true};

void inputThread(rd::IChannel* ch, rd::InputInjector* injector) {
    rd::net::MsgType type;
    std::vector<uint8_t> payload;
    while (g_running.load()) {
        if (!ch->recvMessage(type, payload, -1)) {
            g_running.store(false);
            break;
        }
        if (type == rd::net::MsgType::Input && injector) {
            rd::InputEvent ev;
            if (rd::deserializeInput(payload.data(), payload.size(), ev)) {
                injector->inject(ev);
            }
        }
    }
}

// 推流主循环：采集 -> 编码 -> 发送。
void runHostSession(rd::IChannel& ch, rd::ScreenCapture& cap,
                    rd::H264Encoder& enc, rd::Frame& frame,
                    rd::InputInjector* injector, int fps, long maxFrames) {
    g_running.store(true);
    std::thread th(inputThread, &ch, injector);

    const auto interval = std::chrono::milliseconds(1000 / fps);
    long count = 0;
    std::vector<rd::EncodedPacket> packets;

    while (g_running.load()) {
        const auto t0 = std::chrono::steady_clock::now();

        if (!cap.captureFrame(frame) || frame.empty()) {
            std::fprintf(stderr, "采集失败，停止\n");
            break;
        }
        packets.clear();
        if (!enc.encode(frame, packets)) {
            std::fprintf(stderr, "编码失败，停止\n");
            break;
        }

        bool peerAlive = true;
        for (const auto& p : packets) {
            if (!ch.sendMessage(rd::net::MsgType::Video, p.data.data(),
                                static_cast<uint32_t>(p.data.size()))) {
                peerAlive = false;
                break;
            }
            ++count;
        }
        if (!peerAlive) {
            std::printf("对端断开，停止推流\n");
            break;
        }
        if (maxFrames > 0 && count >= maxFrames) {
            std::printf("已发送 %ld 个包，达到上限，退出\n", count);
            break;
        }

        const auto elapsed = std::chrono::steady_clock::now() - t0;
        if (elapsed < interval) std::this_thread::sleep_for(interval - elapsed);
    }

    g_running.store(false);
    if (th.joinable()) th.join();
}

bool initCaptureEncoder(std::unique_ptr<rd::ScreenCapture>& cap,
                        rd::H264Encoder& enc, rd::Frame& frame, int fps) {
    cap = rd::ScreenCapture::create();
    if (!cap || !cap->init()) {
        std::fprintf(stderr, "采集初始化失败\n");
        return false;
    }
    if (!cap->captureFrame(frame) || frame.empty()) {
        std::fprintf(stderr, "首帧采集失败\n");
        return false;
    }
    rd::H264Encoder::Config cfg;
    cfg.width = frame.width;
    cfg.height = frame.height;
    cfg.fps = fps;
    cfg.bitrateKbps = 8000;
    cfg.gopSize = fps * 2;
    if (!enc.open(cfg)) {
        std::fprintf(stderr, "编码器打开失败\n");
        return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const int fps = 30;
    const bool p2p = (argc > 1) && std::strcmp(argv[1], "--p2p") == 0;

    if (!rd::net::initSockets()) {
        std::fprintf(stderr, "socket 初始化失败\n");
        return 1;
    }

    std::unique_ptr<rd::ScreenCapture> cap;
    rd::H264Encoder enc;
    rd::Frame frame;
    if (!initCaptureEncoder(cap, enc, frame, fps)) return 1;

    auto injector = rd::InputInjector::create();
    if (!injector || !injector->init(frame.width, frame.height)) {
        std::fprintf(stderr, "警告: 输入注入不可用，仅推流不可控制\n");
        injector.reset();
    }

    if (p2p) {
#if defined(RD_HAVE_P2P)
        if (argc < 5) {
            std::fprintf(stderr,
                         "用法: %s --p2p <信令服务器IP> <端口> <id>\n", argv[0]);
            return 1;
        }
        const std::string sigHost = argv[2];
        const uint16_t sigPort = static_cast<uint16_t>(std::atoi(argv[3]));
        const std::string id = argv[4];

        rd::RtcTransport rtc;
        if (!rtc.startHost(sigHost, sigPort, id)) {
            std::fprintf(stderr, "连接信令服务器失败\n");
            return 1;
        }
        std::printf("已注册 id=%s，等待对端连接并打洞...\n", id.c_str());
        if (!rtc.waitConnected(30000)) {
            std::fprintf(stderr, "30s 内未建立 P2P 连接\n");
            return 1;
        }
        std::printf("P2P 连接已建立，开始推流 (%dx%d @ %dfps)\n", frame.width,
                    frame.height, fps);
        runHostSession(rtc, *cap, enc, frame, injector.get(), fps, 0);
        rtc.close();
#else
        std::fprintf(stderr, "本程序未启用 P2P（RD_ENABLE_P2P=OFF）\n");
        return 1;
#endif
    } else {
        const uint16_t port =
            (argc > 1) ? static_cast<uint16_t>(std::atoi(argv[1])) : 9000;
        const long maxFrames = (argc > 2) ? std::atol(argv[2]) : 0;

        rd::net::TcpListener listener;
        if (!listener.listen(port)) {
            std::fprintf(stderr, "监听端口 %u 失败\n", port);
            return 1;
        }
        std::printf("host 已监听 :%u，等待 viewer 连接...\n", port);

        rd::net::TcpConn conn = listener.accept();
        if (!conn.valid()) {
            std::fprintf(stderr, "accept 失败\n");
            return 1;
        }
        std::printf("viewer 已连接，开始推流 (%dx%d @ %dfps)\n", frame.width,
                    frame.height, fps);

        rd::TcpChannel ch(std::move(conn));
        runHostSession(ch, *cap, enc, frame, injector.get(), fps, maxFrames);
    }

    rd::net::shutdownSockets();
    return 0;
}
