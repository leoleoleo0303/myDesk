#include "viewer_session.h"

#include <vector>

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

ViewerSession::ViewerSession(QObject* parent) : QObject(parent) {}

ViewerSession::~ViewerSession() { stop(); }

bool ViewerSession::authenticate(const std::string& password) {
    if (password.empty()) return true;

    if (!ch_->sendMessage(static_cast<net::MsgType>(kMsgAuth),
                          password.data(),
                          static_cast<uint32_t>(password.size()))) {
        return false;
    }

    net::MsgType type;
    std::vector<uint8_t> payload;
    if (!ch_->recvMessage(type, payload, 10000)) return false;

    if (static_cast<uint8_t>(type) == kMsgAuthOk) return true;

    if (static_cast<uint8_t>(type) == kMsgAuthFail) {
        const std::string msg(payload.begin(), payload.end());
        emit authFailed(QString::fromStdString(msg));
    }
    return false;
}

bool ViewerSession::connectLAN(const std::string& host, uint16_t port,
                               const std::string& password) {
    auto conn = net::TcpConn::connect(host, port);
    if (!conn.valid()) {
        emit error("Connection failed");
        return false;
    }
    ch_ = std::make_unique<TcpChannel>(std::move(conn));

    if (!authenticate(password)) {
        ch_.reset();
        return false;
    }

    emit connected();
    return true;
}

bool ViewerSession::connectP2P(const std::string& signalHost,
                               uint16_t signalPort,
                               const std::string& deviceId,
                               const std::string& password) {
#if defined(RD_HAVE_P2P)
    auto rtc = std::make_unique<RtcTransport>();
    if (!rtc->startViewer(signalHost, signalPort, deviceId)) {
        emit error("Signal server connection failed");
        return false;
    }
    if (!rtc->waitConnected(30000)) {
        emit error("P2P connection timeout");
        return false;
    }
    ch_ = std::move(rtc);

    if (!authenticate(password)) {
        ch_.reset();
        return false;
    }

    emit connected();
    return true;
#else
    emit error("P2P not enabled");
    return false;
#endif
}

void ViewerSession::startReceiving() {
    if (!ch_ || running_.load()) return;
    if (!dec_.open()) {
        emit error("Decoder init failed");
        return;
    }
    running_.store(true);
    thread_ = std::thread([this]() { recvLoop(); });
}

void ViewerSession::stop() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
    ch_.reset();
}

void ViewerSession::sendInput(const InputEvent& ev) {
    if (!ch_) return;
    uint8_t buf[kInputEventSize];
    serializeInput(ev, buf);
    ch_->sendMessage(net::MsgType::Input, buf,
                     static_cast<uint32_t>(sizeof(buf)));
}

void ViewerSession::recvLoop() {
    net::MsgType type;
    std::vector<uint8_t> payload;
    Frame frame;

    while (running_.load()) {
        if (!ch_->recvMessage(type, payload, 200)) {
            if (!ch_->isConnected()) break;
            continue;
        }
        if (type != net::MsgType::Video) continue;

        bool got = false;
        if (!dec_.decode(payload.data(), payload.size(), frame, got)) continue;
        if (!got) continue;

        QImage img(frame.data.data(), frame.width, frame.height, frame.stride,
                   QImage::Format_ARGB32);
        emit frameReady(img.copy());
    }
    running_.store(false);
    emit disconnected();
}

}  // namespace rd
