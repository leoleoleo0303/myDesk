#include "remote_session.h"

#include <vector>

namespace rd {

RemoteSession::RemoteSession(QObject* parent) : QObject(parent) {}

RemoteSession::~RemoteSession() { stop(); }

void RemoteSession::start(std::unique_ptr<IChannel> ch) {
    ch_ = std::move(ch);
    if (!dec_.open()) return;
    running_.store(true);
    thread_ = std::thread([this]() { recvLoop(); });
}

void RemoteSession::stop() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
    ch_.reset();
}

void RemoteSession::sendInput(const InputEvent& ev) {
    if (!ch_) return;
    uint8_t buf[kInputEventSize];
    serializeInput(ev, buf);
    ch_->sendMessage(net::MsgType::Input, buf,
                     static_cast<uint32_t>(sizeof(buf)));
}

void RemoteSession::recvLoop() {
    net::MsgType type;
    std::vector<uint8_t> payload;
    Frame frame;

    while (running_.load()) {
        if (!ch_->recvMessage(type, payload, 200)) {
            if (!ch_->isConnected()) break;  // 对端断开
            continue;                        // 仅超时，继续等
        }
        if (type != net::MsgType::Video) continue;

        bool got = false;
        if (!dec_.decode(payload.data(), payload.size(), frame, got)) continue;
        if (!got) continue;

        // BGRA 在小端机上等价于 QImage::Format_ARGB32；copy() 复制出独立数据
        QImage img(frame.data.data(), frame.width, frame.height, frame.stride,
                   QImage::Format_ARGB32);
        emit frameReady(img.copy());
    }
    emit disconnected();
}

}  // namespace rd
