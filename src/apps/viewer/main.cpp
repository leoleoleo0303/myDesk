// viewer（控制端）：接收 -> 解码 -> 显示，并回传鼠标/键盘实现远程控制。
//
// 两种模式：
//   局域网:  viewer <host_ip> [端口]                       直连显示
//            viewer <host_ip> <端口> <帧数> <out.ppm>       无窗口测试
//   公网P2P: viewer --p2p <信令服务器IP> <端口> <id>         经信令配对 + 打洞

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "core/codec/h264_decoder.h"
#include "core/input/input_event.h"
#include "core/net/channel.h"
#include "core/net/message.h"
#include "core/net/tcp_socket.h"

#if defined(RD_HAVE_P2P)
#include "core/p2p/rtc_transport.h"
#endif

#if defined(RD_HAVE_SDL2)
// 不让 SDL 用宏把 main 重定义为 SDL_main（Windows 上否则需链接 SDL2main）。
// 我们自己提供标准 main 入口，跨平台行为一致。
#define SDL_MAIN_HANDLED
#include <SDL.h>
#endif

namespace {

bool savePPM(const char* path, const rd::Frame& f) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << f.width << ' ' << f.height << "\n255\n";
    for (int y = 0; y < f.height; ++y) {
        const uint8_t* row = f.data.data() + static_cast<size_t>(y) * f.stride;
        for (int x = 0; x < f.width; ++x) {
            const uint8_t* px = row + static_cast<size_t>(x) * 4;
            const char rgb[3] = {static_cast<char>(px[2]),
                                 static_cast<char>(px[1]),
                                 static_cast<char>(px[0])};
            out.write(rgb, 3);
        }
    }
    return static_cast<bool>(out);
}

int runHeadless(rd::IChannel& ch, long maxFrames, const char* savePath) {
    rd::H264Decoder dec;
    if (!dec.open()) {
        std::fprintf(stderr, "解码器打开失败\n");
        return 1;
    }
    rd::Frame frame;
    long decoded = 0;
    rd::net::MsgType type;
    std::vector<uint8_t> payload;

    while (decoded < maxFrames) {
        if (!ch.recvMessage(type, payload, -1)) {
            std::fprintf(stderr, "连接结束（已解码 %ld 帧）\n", decoded);
            break;
        }
        if (type != rd::net::MsgType::Video) continue;
        bool got = false;
        if (!dec.decode(payload.data(), payload.size(), frame, got)) {
            std::fprintf(stderr, "解码失败\n");
            return 1;
        }
        if (got) ++decoded;
    }
    if (decoded == 0) {
        std::fprintf(stderr, "未解出任何帧\n");
        return 1;
    }
    std::printf("无窗口模式：成功解码 %ld 帧, 末帧 %dx%d\n", decoded,
                frame.width, frame.height);
    if (savePath && !savePPM(savePath, frame)) {
        std::fprintf(stderr, "保存失败\n");
        return 1;
    }
    if (savePath) std::printf("已保存末帧到 %s\n", savePath);
    return 0;
}

#if defined(RD_HAVE_SDL2)

uint32_t sdlToKeysym(SDL_Keycode k) {
    switch (k) {
        case SDLK_RETURN: return 0xff0d;
        case SDLK_ESCAPE: return 0xff1b;
        case SDLK_BACKSPACE: return 0xff08;
        case SDLK_TAB: return 0xff09;
        case SDLK_DELETE: return 0xffff;
        case SDLK_LEFT: return 0xff51;
        case SDLK_UP: return 0xff52;
        case SDLK_RIGHT: return 0xff53;
        case SDLK_DOWN: return 0xff54;
        case SDLK_HOME: return 0xff50;
        case SDLK_END: return 0xff57;
        case SDLK_PAGEUP: return 0xff55;
        case SDLK_PAGEDOWN: return 0xff56;
        case SDLK_LSHIFT: return 0xffe1;
        case SDLK_RSHIFT: return 0xffe2;
        case SDLK_LCTRL: return 0xffe3;
        case SDLK_RCTRL: return 0xffe4;
        case SDLK_LALT: return 0xffe9;
        case SDLK_RALT: return 0xffea;
        case SDLK_F1: return 0xffbe;
        case SDLK_F2: return 0xffbf;
        case SDLK_F3: return 0xffc0;
        case SDLK_F4: return 0xffc1;
        case SDLK_F5: return 0xffc2;
        case SDLK_F6: return 0xffc3;
        case SDLK_F7: return 0xffc4;
        case SDLK_F8: return 0xffc5;
        case SDLK_F9: return 0xffc6;
        case SDLK_F10: return 0xffc7;
        case SDLK_F11: return 0xffc8;
        case SDLK_F12: return 0xffc9;
        default: break;
    }
    if (k >= 0x20 && k <= 0x7e) return static_cast<uint32_t>(k);
    return 0;
}

void sendInput(rd::IChannel& ch, const rd::InputEvent& ev) {
    uint8_t buf[rd::kInputEventSize];
    rd::serializeInput(ev, buf);
    ch.sendMessage(rd::net::MsgType::Input, buf, static_cast<uint32_t>(sizeof(buf)));
}

int runWindow(rd::IChannel& ch) {
    SDL_SetMainReady();  // 配合 SDL_MAIN_HANDLED 使用
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init 失败: %s\n", SDL_GetError());
        return 1;
    }
    rd::H264Decoder dec;
    if (!dec.open()) {
        std::fprintf(stderr, "解码器打开失败\n");
        SDL_Quit();
        return 1;
    }

    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex = nullptr;
    int texW = 0, texH = 0;

    rd::Frame frame;
    rd::net::MsgType type;
    std::vector<uint8_t> payload;
    bool running = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            int winW = texW, winH = texH;
            if (win) SDL_GetWindowSize(win, &winW, &winH);
            if (winW <= 0) winW = 1;
            if (winH <= 0) winH = 1;

            switch (ev.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_MOUSEMOTION: {
                    rd::InputEvent ie;
                    ie.type = rd::InputType::MouseMove;
                    ie.x = static_cast<uint16_t>(
                        static_cast<long>(ev.motion.x) * 65535 / winW);
                    ie.y = static_cast<uint16_t>(
                        static_cast<long>(ev.motion.y) * 65535 / winH);
                    sendInput(ch, ie);
                    break;
                }
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    rd::InputEvent ie;
                    ie.type = rd::InputType::MouseButton;
                    ie.button = ev.button.button;
                    ie.down = (ev.type == SDL_MOUSEBUTTONDOWN) ? 1 : 0;
                    sendInput(ch, ie);
                    break;
                }
                case SDL_MOUSEWHEEL: {
                    rd::InputEvent ie;
                    ie.type = rd::InputType::MouseWheel;
                    ie.wheel = static_cast<int16_t>(ev.wheel.y);
                    sendInput(ch, ie);
                    break;
                }
                case SDL_KEYDOWN:
                case SDL_KEYUP: {
                    const uint32_t ks = sdlToKeysym(ev.key.keysym.sym);
                    if (ks != 0) {
                        rd::InputEvent ie;
                        ie.type = rd::InputType::Key;
                        ie.key = ks;
                        ie.down = (ev.type == SDL_KEYDOWN) ? 1 : 0;
                        sendInput(ch, ie);
                    }
                    break;
                }
                default:
                    break;
            }
        }
        if (!running) break;

        // 小超时收包，保证无视频时 SDL 事件仍能及时处理
        if (!ch.recvMessage(type, payload, 4)) {
            continue;  // 超时或暂无数据，回到事件循环
        }
        if (type != rd::net::MsgType::Video) continue;

        bool got = false;
        if (!dec.decode(payload.data(), payload.size(), frame, got)) {
            std::fprintf(stderr, "解码失败\n");
            break;
        }
        if (!got) continue;

        if (!win || frame.width != texW || frame.height != texH) {
            if (tex) SDL_DestroyTexture(tex);
            if (!win) {
                win = SDL_CreateWindow("todesk viewer", SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED, frame.width,
                                       frame.height, SDL_WINDOW_RESIZABLE);
                ren = SDL_CreateRenderer(
                    win, -1,
                    SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            }
            tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, frame.width,
                                    frame.height);
            texW = frame.width;
            texH = frame.height;
        }

        SDL_UpdateTexture(tex, nullptr, frame.data.data(), frame.stride);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    }

    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
#endif  // RD_HAVE_SDL2

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr,
                     "用法:\n"
                     "  %s <host_ip> [端口]\n"
                     "  %s <host_ip> <端口> <帧数> <out.ppm>  (无窗口测试)\n"
                     "  %s --p2p <信令服务器IP> <端口> <id>    (公网P2P)\n",
                     argv[0], argv[0], argv[0]);
        return 1;
    }

    if (!rd::net::initSockets()) {
        std::fprintf(stderr, "socket 初始化失败\n");
        return 1;
    }

    const bool p2p = std::strcmp(argv[1], "--p2p") == 0;

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
        if (!rtc.startViewer(sigHost, sigPort, id)) {
            std::fprintf(stderr, "连接信令服务器失败\n");
            return 1;
        }
        std::printf("正在连接 id=%s 并打洞...\n", id.c_str());
        if (!rtc.waitConnected(30000)) {
            std::fprintf(stderr, "30s 内未建立 P2P 连接\n");
            return 1;
        }
        std::printf("P2P 连接已建立\n");
        int rc;
        if (argc >= 7) {
            // 无窗口测试: viewer --p2p <ip> <端口> <id> <帧数> <out.ppm>
            const long frames = std::atol(argv[5]);
            rc = runHeadless(rtc, frames, argv[6]);
        } else {
#if defined(RD_HAVE_SDL2)
            rc = runWindow(rtc);
#else
            std::fprintf(stderr, "未编译 SDL2，无法显示\n");
            rc = 1;
#endif
        }
        rtc.close();
        rd::net::shutdownSockets();
        return rc;
#else
        std::fprintf(stderr, "本程序未启用 P2P（RD_ENABLE_P2P=OFF）\n");
        return 1;
#endif
    }

    // 局域网模式
    const std::string host = argv[1];
    const uint16_t port =
        (argc > 2) ? static_cast<uint16_t>(std::atoi(argv[2])) : 9000;

    rd::net::TcpConn conn = rd::net::TcpConn::connect(host, port);
    if (!conn.valid()) {
        std::fprintf(stderr, "连接 %s:%u 失败\n", host.c_str(), port);
        rd::net::shutdownSockets();
        return 1;
    }
    std::printf("已连接到 %s:%u\n", host.c_str(), port);
    rd::TcpChannel ch(std::move(conn));

    int rc;
    if (argc >= 5) {
        const long frames = std::atol(argv[3]);
        rc = runHeadless(ch, frames, argv[4]);
    } else {
#if defined(RD_HAVE_SDL2)
        rc = runWindow(ch);
#else
        std::fprintf(stderr,
                     "未编译 SDL2，无法窗口显示。请用无窗口模式:\n"
                     "  %s <ip> <端口> <帧数> <out.ppm>\n",
                     argv[0]);
        rc = 1;
#endif
    }

    rd::net::shutdownSockets();
    return rc;
}
