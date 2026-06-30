// codec_demo: 验证编解码闭环。
//   采集一帧 -> H.264 编码 -> H.264 解码 -> 保存为 PPM
// 同时打印原始大小、压缩后大小、压缩比，以及解码还原的分辨率。
//
// 用法: codec_demo [输出文件名]   默认 decoded.ppm

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

#include "core/capture/screen_capture.h"
#include "core/codec/h264_decoder.h"
#include "core/codec/h264_encoder.h"

namespace {

bool savePPM(const char* path, const rd::Frame& f) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << f.width << ' ' << f.height << "\n255\n";
    for (int y = 0; y < f.height; ++y) {
        const uint8_t* row = f.data.data() + static_cast<size_t>(y) * f.stride;
        for (int x = 0; x < f.width; ++x) {
            const uint8_t* px = row + static_cast<size_t>(x) * 4;
            const char rgb[3] = {static_cast<char>(px[2]),
                                 static_cast<char>(px[1]),
                                 static_cast<char>(px[0])};
            out.write(rgb, 3);
        }
    }
    return static_cast<bool>(out);
}

}  // namespace

int main(int argc, char** argv) {
    const char* outPath = (argc > 1) ? argv[1] : "decoded.ppm";

    // 1. 采集
    auto cap = rd::ScreenCapture::create();
    if (!cap || !cap->init()) {
        std::fprintf(stderr, "采集初始化失败\n");
        return 1;
    }
    rd::Frame src;
    if (!cap->captureFrame(src) || src.empty()) {
        std::fprintf(stderr, "采集失败\n");
        return 1;
    }
    std::printf("采集: %dx%d, 原始 %zu 字节\n", src.width, src.height,
                src.data.size());

    // 2. 编码
    rd::H264Encoder enc;
    rd::H264Encoder::Config cfg;
    cfg.width = src.width;
    cfg.height = src.height;
    cfg.fps = 30;
    cfg.bitrateKbps = 4000;
    if (!enc.open(cfg)) {
        std::fprintf(stderr, "编码器打开失败\n");
        return 1;
    }

    std::vector<rd::EncodedPacket> packets;
    if (!enc.encode(src, packets)) {
        std::fprintf(stderr, "编码失败\n");
        return 1;
    }
    enc.flush(packets);  // 取出残留包

    size_t encodedBytes = 0;
    for (const auto& p : packets) encodedBytes += p.data.size();
    std::printf("编码: %zu 个包, 共 %zu 字节, 压缩比 %.1fx\n", packets.size(),
                encodedBytes,
                encodedBytes ? static_cast<double>(src.data.size()) / encodedBytes
                             : 0.0);

    // 3. 解码
    rd::H264Decoder dec;
    if (!dec.open()) {
        std::fprintf(stderr, "解码器打开失败\n");
        return 1;
    }
    rd::Frame decoded;
    bool got = false;
    for (const auto& p : packets) {
        if (!dec.decode(p.data.data(), p.data.size(), decoded, got)) {
            std::fprintf(stderr, "解码失败\n");
            return 1;
        }
        if (got) break;
    }
    if (!got) {
        std::fprintf(stderr, "未解出帧\n");
        return 1;
    }
    std::printf("解码: %dx%d, 还原 %zu 字节\n", decoded.width, decoded.height,
                decoded.data.size());

    // 4. 保存
    if (!savePPM(outPath, decoded)) {
        std::fprintf(stderr, "保存失败\n");
        return 1;
    }
    std::printf("已保存解码画面到 %s\n", outPath);
    return 0;
}
