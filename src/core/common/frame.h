#pragma once

#include <cstdint>
#include <vector>

namespace rd {

// 像素格式。采集层统一归一化到 BGRA，方便后续编码/显示。
enum class PixelFormat {
    BGRA,  // 每像素 4 字节，顺序 B, G, R, A
    RGBA,
};

// 一帧画面：与平台无关的原始位图。
struct Frame {
    int width = 0;
    int height = 0;
    int stride = 0;                       // 每行字节数（= width * bytesPerPixel）
    PixelFormat format = PixelFormat::BGRA;
    std::vector<uint8_t> data;            // 像素数据

    bool empty() const {
        return data.empty() || width <= 0 || height <= 0;
    }

    static constexpr int bytesPerPixel() { return 4; }
};

}  // namespace rd
