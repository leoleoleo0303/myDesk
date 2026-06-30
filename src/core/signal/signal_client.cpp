#include "core/signal/signal_client.h"

#include "core/net/message.h"

namespace rd {

SignalClient::~SignalClient() { stop(); }

bool SignalClient::connect(const std::string& host, uint16_t port) {
    conn_ = net::TcpConn::connect(host, port);
    return conn_.valid();
}

bool SignalClient::sendRegister(const std::string& id) {
    return net::sendMessage(conn_, net::MsgType::SigRegister, id.data(),
                            static_cast<uint32_t>(id.size()));
}

bool SignalClient::sendConnect(const std::string& id) {
    return net::sendMessage(conn_, net::MsgType::SigConnect, id.data(),
                            static_cast<uint32_t>(id.size()));
}

bool SignalClient::sendRelay(const std::string& text) {
    return net::sendMessage(conn_, net::MsgType::SigRelay, text.data(),
                            static_cast<uint32_t>(text.size()));
}

void SignalClient::start(PairedCb onPaired, RelayCb onRelay, ErrorCb onError) {
    running_.store(true);
    thread_ = std::thread([this, onPaired, onRelay, onError]() {
        net::MsgType type;
        std::vector<uint8_t> payload;
        while (running_.load()) {
            if (!net::recvMessage(conn_, type, payload)) break;
            const std::string text(payload.begin(), payload.end());
            switch (type) {
                case net::MsgType::SigPaired:
                    if (onPaired) onPaired();
                    break;
                case net::MsgType::SigRelay:
                    if (onRelay) onRelay(text);
                    break;
                case net::MsgType::SigError:
                    if (onError) onError(text);
                    break;
                default:
                    break;
            }
        }
        running_.store(false);
    });
}

void SignalClient::stop() {
    running_.store(false);
    conn_.close();
    if (thread_.joinable()) thread_.join();
}

}  // namespace rd
