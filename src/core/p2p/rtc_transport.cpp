#include "core/p2p/rtc_transport.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <variant>

#include <rtc/rtc.hpp>

namespace rd {

RtcTransport::~RtcTransport() { close(); }

void RtcTransport::setupPeer(bool offerer) {
    rtc::Configuration config;
    config.iceServers.emplace_back(iceServer_);
    // 提高单条消息上限：H.264 关键帧可达数十~数百 KB，默认上限会被超过。
    // 两端都设置，握手时取双方的较小值。
    config.maxMessageSize = 4 * 1024 * 1024;
    pc_ = std::make_shared<rtc::PeerConnection>(config);

    // 本地 SDP 描述生成后，通过信令发给对端
    pc_->onLocalDescription([this](rtc::Description desc) {
        const std::string msg = "DESC " + std::string(desc.typeString()) +
                                "\n" + std::string(desc);
        signal_.sendRelay(msg);
    });

    // 本地 ICE 候选生成后，通过信令发给对端
    pc_->onLocalCandidate([this](rtc::Candidate cand) {
        const std::string msg =
            "CAND " + cand.mid() + "\n" + std::string(cand);
        signal_.sendRelay(msg);
    });

    pc_->onStateChange([](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Failed) {
            std::fprintf(stderr, "[rtc] 连接失败\n");
        }
    });

    if (!offerer) {
        // answerer：对端创建的 DataChannel 会通过这里到达
        pc_->onDataChannel([this](std::shared_ptr<rtc::DataChannel> dc) {
            std::lock_guard<std::mutex> lk(dcMtx_);
            dc_ = dc;
            bindDataChannel(dc_);
        });
    }
}

void RtcTransport::bindDataChannel(std::shared_ptr<rtc::DataChannel> dc) {
    dc->onOpen([this]() {
        connected_.store(true);
        connCv_.notify_all();
    });
    dc->onClosed([this]() {
        connected_.store(false);
        qCv_.notify_all();
    });
    dc->onMessage([this](rtc::message_variant msg) {
        if (!std::holds_alternative<rtc::binary>(msg)) return;
        const rtc::binary& bin = std::get<rtc::binary>(msg);
        if (bin.empty()) return;

        const auto type =
            static_cast<net::MsgType>(std::to_integer<uint8_t>(bin[0]));
        std::vector<uint8_t> payload(bin.size() - 1);
        for (size_t i = 1; i < bin.size(); ++i) {
            payload[i - 1] = std::to_integer<uint8_t>(bin[i]);
        }
        {
            std::lock_guard<std::mutex> lk(qMtx_);
            queue_.emplace_back(type, std::move(payload));
        }
        qCv_.notify_one();
    });
}

void RtcTransport::onRelayText(const std::string& text) {
    const auto nl = text.find('\n');
    if (nl == std::string::npos) return;
    const std::string header = text.substr(0, nl);
    const std::string body = text.substr(nl + 1);

    try {
        if (header.rfind("DESC ", 0) == 0) {
            const std::string type = header.substr(5);  // offer / answer
            pc_->setRemoteDescription(rtc::Description(body, type));
        } else if (header.rfind("CAND ", 0) == 0) {
            const std::string mid = header.substr(5);
            pc_->addRemoteCandidate(rtc::Candidate(body, mid));
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[rtc] 处理信令失败: %s\n", e.what());
    }
}

bool RtcTransport::startHost(const std::string& signalHost,
                             uint16_t signalPort, const std::string& id) {
    if (!signal_.connect(signalHost, signalPort)) return false;
    setupPeer(/*offerer=*/false);

    signal_.start(
        /*onPaired=*/[]() { std::printf("[rtc] 已配对，等待对端 offer...\n"); },
        /*onRelay=*/[this](const std::string& t) { onRelayText(t); },
        /*onError=*/
        [](const std::string& e) {
            std::fprintf(stderr, "[rtc] 信令错误: %s\n", e.c_str());
        });

    return signal_.sendRegister(id);
}

bool RtcTransport::startViewer(const std::string& signalHost,
                               uint16_t signalPort, const std::string& id) {
    if (!signal_.connect(signalHost, signalPort)) return false;
    setupPeer(/*offerer=*/true);

    signal_.start(
        /*onPaired=*/
        [this]() {
            std::printf("[rtc] 已配对，发起 offer...\n");
            // 配对后再创建 DataChannel，触发 offer 生成（避免过早发送信令）
            std::lock_guard<std::mutex> lk(dcMtx_);
            dc_ = pc_->createDataChannel("rd");
            bindDataChannel(dc_);
        },
        /*onRelay=*/[this](const std::string& t) { onRelayText(t); },
        /*onError=*/
        [](const std::string& e) {
            std::fprintf(stderr, "[rtc] 信令错误: %s\n", e.c_str());
        });

    return signal_.sendConnect(id);
}

bool RtcTransport::waitConnected(int timeoutMs) {
    std::unique_lock<std::mutex> lk(connMtx_);
    auto pred = [this]() { return connected_.load() || closing_.load(); };
    if (timeoutMs < 0) {
        connCv_.wait(lk, pred);
    } else {
        connCv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), pred);
    }
    return connected_.load();
}

bool RtcTransport::sendMessage(net::MsgType type, const void* data,
                               uint32_t len) {
    std::lock_guard<std::mutex> lk(dcMtx_);
    if (!dc_) return false;

    std::vector<std::byte> buf(static_cast<size_t>(len) + 1);
    buf[0] = static_cast<std::byte>(static_cast<uint8_t>(type));
    const auto* src = static_cast<const uint8_t*>(data);
    for (uint32_t i = 0; i < len; ++i) {
        buf[i + 1] = static_cast<std::byte>(src[i]);
    }
    try {
        return dc_->send(buf.data(), buf.size());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "[rtc] 发送失败: %s\n", e.what());
        return false;
    }
}

bool RtcTransport::recvMessage(net::MsgType& type, std::vector<uint8_t>& payload,
                               int timeoutMs) {
    std::unique_lock<std::mutex> lk(qMtx_);
    auto pred = [this]() {
        return !queue_.empty() || closing_.load() || !connected_.load();
    };
    if (timeoutMs < 0) {
        qCv_.wait(lk, pred);
    } else if (!qCv_.wait_for(lk, std::chrono::milliseconds(timeoutMs), pred)) {
        return false;
    }
    if (queue_.empty()) return false;  // 已断开

    auto item = std::move(queue_.front());
    queue_.pop_front();
    type = item.first;
    payload = std::move(item.second);
    return true;
}

void RtcTransport::close() {
    closing_.store(true);
    signal_.stop();
    {
        std::lock_guard<std::mutex> lk(dcMtx_);
        if (dc_) {
            try {
                dc_->close();
            } catch (...) {
            }
            dc_.reset();
        }
    }
    if (pc_) {
        try {
            pc_->close();
        } catch (...) {
        }
        pc_.reset();
    }
    qCv_.notify_all();
    connCv_.notify_all();
}

}  // namespace rd
