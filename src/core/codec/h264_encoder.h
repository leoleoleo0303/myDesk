#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/common/frame.h"

namespace rd {

// 一个编码后的 H.264 数据包（Annex-B 字节流，关键帧前自带 SPS/PPS）。
struct EncodedPacket {
    std::vector<uint8_t> data;
    bool keyframe = false;
    int64_t pts = 0;
};

// H.264 编码器（默认用 libx264 软件编码，zerolatency 调优）。
// 输入 BGRA 的 Frame，输出 H.264 码流包。
class H264Encoder {
public:
    struct Config {
        int width = 0;
        int height = 0;
        int fps = 30;
        int bitrateKbps = 4000;
        int gopSize = 60;  // 关键帧间隔（帧数）
    };

    H264Encoder();
    ~H264Encoder();

    H264Encoder(const H264Encoder&) = delete;
    H264Encoder& operator=(const H264Encoder&) = delete;

    bool open(const Config& cfg);

    // 编码一帧。产生的包追加到 out（一帧可能产出 0 个或多个包）。
    bool encode(const Frame& frame, std::vector<EncodedPacket>& out);

    // 冲洗编码器内部缓冲，取出剩余包（结束时调用一次）。
    bool flush(std::vector<EncodedPacket>& out);

    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rd
