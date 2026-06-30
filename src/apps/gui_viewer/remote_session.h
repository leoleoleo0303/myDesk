#pragma once

#include <QImage>
#include <QObject>

#include <atomic>
#include <memory>
#include <thread>

#include "core/codec/h264_decoder.h"
#include "core/input/input_event.h"
#include "core/net/channel.h"

namespace rd {

// 一次远程会话：拥有传输通道，后台线程收包+解码，把帧通过信号发到 UI 线程；
// 同时把 UI 线程产生的输入事件发回对端。
class RemoteSession : public QObject {
    Q_OBJECT
public:
    explicit RemoteSession(QObject* parent = nullptr);
    ~RemoteSession() override;

    // 接管通道所有权并启动接收线程。
    void start(std::unique_ptr<IChannel> ch);
    void stop();

    // 由 UI 线程调用，发送输入事件（底层发送是线程安全的）。
    void sendInput(const InputEvent& ev);

signals:
    void frameReady(const QImage& img);
    void disconnected();

private:
    void recvLoop();

    std::unique_ptr<IChannel> ch_;
    H264Decoder dec_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

}  // namespace rd
