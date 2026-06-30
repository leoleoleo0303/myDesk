#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "core/net/channel.h"
#include "core/net/message.h"
#include "core/signal/signal_client.h"

// 前向声明，避免把 libdatachannel 头暴露给使用方
namespace rtc {
class PeerConnection;
class DataChannel;
}  // namespace rtc

namespace rd {

// 基于 libdatachannel 的 P2P 传输。
//   host(被控端)   = answerer：注册 id，等待对端发来 offer
//   viewer(控制端) = offerer：连接 id，配对后发起 offer
// 建立 DataChannel 后，对外提供与 net 层一致的 sendMessage/recvMessage，
// 这样 host/viewer 的视频/输入协议可以原样跑在 P2P 通道上。
class RtcTransport : public IChannel {
public:
    RtcTransport() = default;
    ~RtcTransport();

    RtcTransport(const RtcTransport&) = delete;
    RtcTransport& operator=(const RtcTransport&) = delete;

    // 启动（异步建立连接）。成功仅表示信令已就绪，真正连通用 waitConnected 等待。
    bool startHost(const std::string& signalHost, uint16_t signalPort,
                   const std::string& id);
    bool startViewer(const std::string& signalHost, uint16_t signalPort,
                     const std::string& id);

    // 等待 DataChannel 打开。timeoutMs<0 表示一直等。
    bool waitConnected(int timeoutMs);
    bool connected() const { return connected_.load(); }

    // 发送一条消息（type + payload）。DataChannel 保留消息边界。
    bool sendMessage(net::MsgType type, const void* data,
                     uint32_t len) override;

    // 阻塞接收一条消息。超时或已断开返回 false。
    bool recvMessage(net::MsgType& type, std::vector<uint8_t>& payload,
                     int timeoutMs) override;

    bool isConnected() const override { return connected_.load(); }

    void close();

private:
    void setupPeer(bool offerer);
    void bindDataChannel(std::shared_ptr<rtc::DataChannel> dc);
    void onRelayText(const std::string& text);

    SignalClient signal_;
    std::shared_ptr<rtc::PeerConnection> pc_;
    std::shared_ptr<rtc::DataChannel> dc_;
    std::mutex dcMtx_;

    std::mutex qMtx_;
    std::condition_variable qCv_;
    std::deque<std::pair<net::MsgType, std::vector<uint8_t>>> queue_;

    std::mutex connMtx_;
    std::condition_variable connCv_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> closing_{false};

    // ICE 服务器（STUN）。国内部署建议换成自建 coturn。
    std::string iceServer_ = "stun:stun.l.google.com:19302";
};

}  // namespace rd
