// signal_server（信令服务器）：部署在公网服务器上，负责牵线搭桥。
//
// 流程：
//   host   连接后发送 SigRegister(id)  -> 服务器登记该 id，等待对端
//   viewer 连接后发送 SigConnect(id)   -> 服务器找到对应 host，两端配对
//   配对成功后，双方各收到 SigPaired
//   之后任一端发来的 SigRelay 都会被原样转发给对端（用于交换打洞信息）
//   任一端断开 -> 关闭对端连接，清理
//
// 协议复用 rdnet 的消息分帧（[type:1][len:4 BE][payload]），不依赖
// FFmpeg/X11，可在最小化服务器上单独构建。
//
// 用法: signal_server [端口]   默认 7000

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/net/message.h"
#include "core/net/tcp_socket.h"

using rd::net::MsgType;
using rd::net::TcpConn;

namespace {

struct Session {
    TcpConn conn;
    std::string id;
    std::mutex sendMtx;                  // 串行化对本连接的发送
    std::mutex partnerMtx;               // 保护 partner
    std::shared_ptr<Session> partner;    // 已配对的对端
    std::atomic<bool> alive{true};

    explicit Session(TcpConn c) : conn(std::move(c)) {}

    bool send(MsgType t, const void* d, uint32_t n) {
        std::lock_guard<std::mutex> lk(sendMtx);
        return rd::net::sendMessage(conn, t, d, n);
    }
    std::shared_ptr<Session> getPartner() {
        std::lock_guard<std::mutex> lk(partnerMtx);
        return partner;
    }
    void setPartner(std::shared_ptr<Session> p) {
        std::lock_guard<std::mutex> lk(partnerMtx);
        partner = std::move(p);
    }
};

std::mutex g_regMtx;
std::map<std::string, std::shared_ptr<Session>> g_waiting;  // id -> 等待中的 host

void sendError(Session& s, const char* msg) {
    s.send(MsgType::SigError, msg, static_cast<uint32_t>(std::strlen(msg)));
}

void handleClient(std::shared_ptr<Session> s) {
    MsgType type;
    std::vector<uint8_t> payload;

    // 第一条消息：注册 或 连接
    if (!rd::net::recvMessage(s->conn, type, payload)) return;
    const std::string id(payload.begin(), payload.end());

    if (type == MsgType::SigRegister) {
        s->id = id;
        {
            std::lock_guard<std::mutex> lk(g_regMtx);
            g_waiting[id] = s;
        }
        std::printf("[signal] host 注册 id=%s，等待 viewer...\n", id.c_str());

    } else if (type == MsgType::SigConnect) {
        std::shared_ptr<Session> host;
        {
            std::lock_guard<std::mutex> lk(g_regMtx);
            auto it = g_waiting.find(id);
            if (it != g_waiting.end()) {
                host = it->second;
                g_waiting.erase(it);  // id 被消费
            }
        }
        if (!host) {
            std::printf("[signal] viewer 请求 id=%s 不存在\n", id.c_str());
            sendError(*s, "no such id");
            return;
        }
        // 互相设为对端
        s->setPartner(host);
        host->setPartner(s);
        std::printf("[signal] 配对成功 id=%s\n", id.c_str());
        host->send(MsgType::SigPaired, nullptr, 0);
        s->send(MsgType::SigPaired, nullptr, 0);

    } else {
        sendError(*s, "expected register/connect");
        return;
    }

    // 转发循环：把 SigRelay 透传给对端
    while (s->alive.load()) {
        if (!rd::net::recvMessage(s->conn, type, payload)) break;
        if (type == MsgType::SigRelay) {
            auto p = s->getPartner();
            if (p && !p->send(MsgType::SigRelay, payload.data(),
                              static_cast<uint32_t>(payload.size()))) {
                break;
            }
        }
    }

    // 清理：通知并关闭对端，从等待表移除
    s->alive.store(false);
    if (auto p = s->getPartner()) {
        p->alive.store(false);
        p->conn.close();
    }
    if (!s->id.empty()) {
        std::lock_guard<std::mutex> lk(g_regMtx);
        auto it = g_waiting.find(s->id);
        if (it != g_waiting.end() && it->second.get() == s.get()) {
            g_waiting.erase(it);
        }
    }
    std::printf("[signal] 连接关闭 id=%s\n", s->id.c_str());
}

}  // namespace

int main(int argc, char** argv) {
    const uint16_t port =
        (argc > 1) ? static_cast<uint16_t>(std::atoi(argv[1])) : 7000;

    if (!rd::net::initSockets()) {
        std::fprintf(stderr, "socket 初始化失败\n");
        return 1;
    }

    rd::net::TcpListener listener;
    if (!listener.listen(port)) {
        std::fprintf(stderr, "监听端口 %u 失败\n", port);
        return 1;
    }
    std::printf("signal_server 已启动，监听 :%u\n", port);

    while (true) {
        TcpConn conn = listener.accept();
        if (!conn.valid()) continue;
        auto s = std::make_shared<Session>(std::move(conn));
        std::thread(handleClient, s).detach();
    }

    rd::net::shutdownSockets();
    return 0;
}
