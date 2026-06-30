#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "core/common/frame.h"

namespace rd {

// H.264 解码器。输入 H.264 码流包，输出 BGRA 的 Frame。
class H264Decoder {
public:
    H264Decoder();
    ~H264Decoder();

    H264Decoder(const H264Decoder&) = delete;
    H264Decoder& operator=(const H264Decoder&) = delete;

    bool open();

    // 解码一个 H.264 包。若解出完整一帧，写入 outFrame 并令 gotFrame=true。
    // 解码器可能需要多个包才吐出一帧，此时返回 true 但 gotFrame=false。
    bool decode(const uint8_t* data, size_t size, Frame& outFrame,
                bool& gotFrame);

    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rd
