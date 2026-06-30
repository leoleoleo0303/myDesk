#pragma once

#include <QObject>
#include <QString>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/capture/screen_capture.h"
#include "core/codec/h264_encoder.h"
#include "core/input/input_event.h"
#include "core/input/input_injector.h"
#include "core/net/channel.h"

namespace rd {

// Host service: listens for incoming connections, streams screen, accepts input.
// Supports simultaneous LAN (TCP listener) and P2P (signal_server registration).
class HostService : public QObject {
    Q_OBJECT
public:
    explicit HostService(QObject* parent = nullptr);
    ~HostService() override;

    void setPassword(const std::string& pwd) { password_ = pwd; }

    // Start LAN listener on the given port
    bool startListening(uint16_t port = 9000);

    // Register device ID with signal_server for P2P discovery.
    // Can run concurrently with startListening.
    bool registerP2P(const std::string& signalHost, uint16_t signalPort,
                     const std::string& deviceId);

    void stop();
    bool isRunning() const { return running_.load(); }
    bool isP2PRegistered() const { return p2pRegistered_.load(); }
    int clientCount() const { return clientCount_.load(); }

signals:
    void clientConnected(const QString& info);
    void clientDisconnected();
    void p2pRegistered();
    void p2pRegisterFailed(const QString& msg);
    void error(const QString& msg);

private:
    void listenLoop();
    void p2pLoop(std::string signalHost, uint16_t signalPort,
                 std::string deviceId);
    void handleClient(std::unique_ptr<IChannel> ch);
    bool authenticateClient(IChannel& ch);

    std::string password_;
    uint16_t port_ = 9000;
    std::atomic<bool> running_{false};
    std::atomic<bool> p2pRegistered_{false};
    std::atomic<int> clientCount_{0};
    std::thread listenThread_;
    std::thread p2pThread_;
    std::vector<std::thread> clientThreads_;
    std::mutex threadsMtx_;
};

}  // namespace rd
