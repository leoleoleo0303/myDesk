#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace rd {

// 屏幕录制器：采集屏幕 + H.264 编码 + MP4 封装，保存到本地文件。
// 使用方法：
//   ScreenRecorder rec;
//   rec.start("output.mp4");   // 开始录制
//   ...
//   rec.stop();                // 停止录制并关闭文件
class ScreenRecorder {
public:
    struct Config {
        int fps = 30;           // 录制帧率
        int bitrateKbps = 8000; // 码率 (kbps)
        int gopSize = 60;       // 关键帧间隔
    };

    ScreenRecorder();
    ~ScreenRecorder();

    ScreenRecorder(const ScreenRecorder&) = delete;
    ScreenRecorder& operator=(const ScreenRecorder&) = delete;

    // 开始录制，输出到 outputPath（MP4 格式）。
    // 成功返回 true，失败返回 false。
    bool start(const std::string& outputPath, const Config& cfg = Config{});

    // 停止录制并关闭文件。
    void stop();

    bool isRecording() const { return recording_.load(); }

    // 录制时长（秒）
    double durationSeconds() const;

    // 错误回调
    void setErrorCallback(std::function<void(const std::string&)> cb) {
        errorCb_ = std::move(cb);
    }

private:
    void recordLoop(const std::string& outputPath, Config cfg);

    std::atomic<bool> recording_{false};
    std::atomic<int64_t> frameCount_{0};
    int fps_ = 30;
    std::thread thread_;
    std::function<void(const std::string&)> errorCb_;
};

}  // namespace rd
