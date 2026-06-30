#pragma once

#include <QObject>
#include <QByteArray>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "core/net/channel.h"

namespace rd {

// 音频管理器 - 语音通话支持
// 录制本地麦克风音频 -> 发送到对端
// 接收对端音频 -> 播放到本地扬声器
//
// 音频格式: PCM 16-bit mono 16kHz (后续可升级到 Opus)
// 现在使用简单的 PCM 传输，后期可集成 Opus 编解码
class AudioManager : public QObject {
    Q_OBJECT
public:
    explicit AudioManager(QObject* parent = nullptr);
    ~AudioManager() override;

    void setChannel(IChannel* ch) { channel_ = ch; }

    // 开始/停止本地录音并发送
    void startCapture();
    void stopCapture();
    bool isCapturing() const { return capturing_.load(); }

    // 开始/停止播放远程音频
    void startPlayback();
    void stopPlayback();
    bool isPlaying() const { return playing_.load(); }

    // 静音/取消静音
    void setMuted(bool muted) { muted_.store(muted); }
    bool isMuted() const { return muted_.load(); }

    // 处理收到的音频消息
    void handleMessage(net::MsgType type, const std::vector<uint8_t>& payload);

    // 发送音频控制命令
    void sendControl(const std::string& action);

signals:
    void captureStarted();
    void captureStopped();
    void playbackStarted();
    void playbackStopped();
    void audioLevelChanged(float level);  // 0.0 - 1.0 音量电平
    void remoteControlReceived(const QString& action);

private:
    void captureLoop();
    void playAudio(const std::vector<uint8_t>& pcmData);

    IChannel* channel_ = nullptr;
    std::atomic<bool> capturing_{false};
    std::atomic<bool> playing_{false};
    std::atomic<bool> muted_{false};
    std::thread captureThread_;

    // PCM 参数
    static constexpr int kSampleRate = 16000;
    static constexpr int kChannels = 1;
    static constexpr int kBitsPerSample = 16;
    static constexpr int kFrameDurationMs = 20;  // 20ms per frame
    static constexpr int kFrameSamples = kSampleRate * kFrameDurationMs / 1000;
};

}  // namespace rd
