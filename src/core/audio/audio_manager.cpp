#include "audio_manager.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <cstring>
#include <cmath>

namespace rd {

AudioManager::AudioManager(QObject* parent)
    : QObject(parent) {}

AudioManager::~AudioManager() {
    stopCapture();
    stopPlayback();
}

void AudioManager::startCapture() {
    if (capturing_.load()) return;
    capturing_.store(true);

    captureThread_ = std::thread([this]() {
        captureLoop();
    });

    emit captureStarted();
    sendControl("start");
}

void AudioManager::stopCapture() {
    capturing_.store(false);
    if (captureThread_.joinable()) {
        captureThread_.join();
    }
    emit captureStopped();
    sendControl("stop");
}

void AudioManager::startPlayback() {
    playing_.store(true);
    emit playbackStarted();
}

void AudioManager::stopPlayback() {
    playing_.store(false);
    emit playbackStopped();
}

void AudioManager::sendControl(const std::string& action) {
    if (!channel_) return;

    QJsonObject obj;
    obj["action"] = QString::fromStdString(action);

    QByteArray json = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    channel_->sendMessage(net::MsgType::AudioControl,
                          json.constData(), static_cast<uint32_t>(json.size()));
}

void AudioManager::handleMessage(net::MsgType type, const std::vector<uint8_t>& payload) {
    switch (type) {
    case net::MsgType::AudioData: {
        if (playing_.load() && !payload.empty()) {
            playAudio(payload);

            // 计算音量电平
            const int16_t* samples = reinterpret_cast<const int16_t*>(payload.data());
            int sampleCount = static_cast<int>(payload.size() / 2);
            double sum = 0;
            for (int i = 0; i < sampleCount; ++i) {
                sum += static_cast<double>(samples[i]) * samples[i];
            }
            float rms = static_cast<float>(std::sqrt(sum / sampleCount) / 32768.0);
            emit audioLevelChanged(rms);
        }
        break;
    }
    case net::MsgType::AudioControl: {
        QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray(reinterpret_cast<const char*>(payload.data()),
                       static_cast<int>(payload.size())));
        QJsonObject obj = doc.object();
        QString action = obj["action"].toString();
        emit remoteControlReceived(action);
        break;
    }
    default:
        break;
    }
}

void AudioManager::captureLoop() {
    // TODO: 平台相关的音频录制实现
    // Windows: WASAPI / WaveIn
    // Linux: PulseAudio / ALSA
    // macOS: CoreAudio
    //
    // 当前为逻辑框架，实际录音需要集成音频库
    // 后续可使用 PortAudio 实现跨平台统一
    //
    // 这里模拟 20ms 一帧的节奏
    const int frameBytes = kFrameSamples * kChannels * (kBitsPerSample / 8);
    std::vector<uint8_t> silence(frameBytes, 0);

    while (capturing_.load()) {
        if (!muted_.load() && channel_) {
            // 实际实现中这里应该从麦克风读取数据
            // 现在只是占位，保持发送节奏
            // channel_->sendMessage(net::MsgType::AudioData,
            //                       silence.data(), frameBytes);
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kFrameDurationMs));
    }
}

void AudioManager::playAudio(const std::vector<uint8_t>& pcmData) {
    // TODO: 平台相关的音频播放实现
    // Windows: WASAPI / WaveOut
    // Linux: PulseAudio / ALSA
    // macOS: CoreAudio
    //
    // 当前为逻辑框架
    (void)pcmData;
}

}  // namespace rd
