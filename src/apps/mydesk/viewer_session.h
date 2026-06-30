#pragma once

#include <QImage>
#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "core/codec/h264_decoder.h"
#include "core/input/input_event.h"
#include "core/net/channel.h"

namespace rd {

// Viewer session: connects to a remote host, receives video, sends input.
class ViewerSession : public QObject {
    Q_OBJECT
public:
    explicit ViewerSession(QObject* parent = nullptr);
    ~ViewerSession() override;

    bool connectLAN(const std::string& host, uint16_t port,
                    const std::string& password);
    bool connectP2P(const std::string& signalHost, uint16_t signalPort,
                    const std::string& deviceId, const std::string& password);

    void startReceiving();
    void stop();
    bool isRunning() const { return running_.load(); }

    void sendInput(const InputEvent& ev);

signals:
    void frameReady(const QImage& img);
    void connected();
    void disconnected();
    void authFailed(const QString& reason);
    void error(const QString& msg);

private:
    bool authenticate(const std::string& password);
    void recvLoop();

    std::unique_ptr<IChannel> ch_;
    H264Decoder dec_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

}  // namespace rd
