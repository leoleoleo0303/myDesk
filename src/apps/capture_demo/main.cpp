// capture_demo: 抓取一帧屏幕并保存为 PPM 图片，用于验证采集链路。
//
// 用法:
//   capture_demo [输出文件名]      默认 screenshot.ppm
//
// PPM 是无压缩的纯文本头 + 原始 RGB，任何看图软件/ImageMagick 都能打开:
//   convert screenshot.ppm screenshot.png

#include <cstdint>
#include <cstdio>
#include <fstream>

#include "core/capture/screen_capture.h"

namespace {

// 把 BGRA 的 Frame 写成 P6 PPM（RGB）。
bool savePPM(const char* path, const rd::Frame& f) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out << "P6\n" << f.width << ' ' << f.height << "\n255\n";
    for (int y = 0; y < f.height; ++y) {
        const uint8_t* row =
            f.data.data() + static_cast<size_t>(y) * f.stride;
        for (int x = 0; x < f.width; ++x) {
            const uint8_t* px = row + static_cast<size_t>(x) * 4;
            const char rgb[3] = {static_cast<char>(px[2]),   // R
                                 static_cast<char>(px[1]),   // G
                                 static_cast<char>(px[0])};  // B
            out.write(rgb, 3);
        }
    }
    return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char** argv) {
    const char* outPath = (argc > 1) ? argv[1] : "screenshot.ppm";

    auto cap = rd::ScreenCapture::create();
    if (!cap) {
        std::fprintf(stderr, "当前平台没有可用的屏幕采集实现\n");
        return 1;
    }
    if (!cap->init()) {
        std::fprintf(stderr, "采集器初始化失败（是否有可用的显示环境/DISPLAY?）\n");
        return 1;
    }

    rd::Frame frame;
    if (!cap->captureFrame(frame) || frame.empty()) {
        std::fprintf(stderr, "采集失败\n");
        return 1;
    }

    std::printf("采集成功: %dx%d, %zu 字节\n", frame.width, frame.height,
                frame.data.size());

    if (!savePPM(outPath, frame)) {
        std::fprintf(stderr, "保存失败: %s\n", outPath);
        return 1;
    }

    std::printf("已保存到 %s\n", outPath);
    return 0;
}
