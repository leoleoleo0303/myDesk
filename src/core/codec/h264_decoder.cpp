#include "core/codec/h264_decoder.h"

#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

namespace rd {

struct H264Decoder::Impl {
    const AVCodec* codec = nullptr;
    AVCodecContext* ctx = nullptr;
    AVFrame* yuv = nullptr;
    AVPacket* pkt = nullptr;
    SwsContext* sws = nullptr;  // YUV420P -> BGRA
    int swsW = 0;
    int swsH = 0;

    ~Impl() { cleanup(); }

    void cleanup() {
        if (sws) {
            sws_freeContext(sws);
            sws = nullptr;
        }
        if (pkt) av_packet_free(&pkt);
        if (yuv) av_frame_free(&yuv);
        if (ctx) avcodec_free_context(&ctx);
    }
};

H264Decoder::H264Decoder() : impl_(std::make_unique<Impl>()) {}
H264Decoder::~H264Decoder() = default;

bool H264Decoder::open() {
    auto& d = *impl_;
    d.codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!d.codec) return false;

    d.ctx = avcodec_alloc_context3(d.codec);
    if (!d.ctx) return false;
    if (avcodec_open2(d.ctx, d.codec, nullptr) < 0) return false;

    d.yuv = av_frame_alloc();
    d.pkt = av_packet_alloc();
    return d.yuv && d.pkt;
}

bool H264Decoder::decode(const uint8_t* data, size_t size, Frame& outFrame,
                         bool& gotFrame) {
    gotFrame = false;
    auto& d = *impl_;
    if (!d.ctx) return false;

    // 用包装的方式把外部内存喂给解码器（拷贝进 AVPacket 缓冲）。
    if (av_new_packet(d.pkt, static_cast<int>(size)) < 0) return false;
    std::memcpy(d.pkt->data, data, size);

    const int sret = avcodec_send_packet(d.ctx, d.pkt);
    av_packet_unref(d.pkt);
    if (sret < 0) return false;

    const int ret = avcodec_receive_frame(d.ctx, d.yuv);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return true;  // 需更多数据
    if (ret < 0) return false;

    const int w = d.yuv->width;
    const int h = d.yuv->height;

    if (!d.sws || d.swsW != w || d.swsH != h) {
        if (d.sws) sws_freeContext(d.sws);
        d.sws = sws_getContext(w, h, static_cast<AVPixelFormat>(d.yuv->format),
                               w, h, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr,
                               nullptr, nullptr);
        d.swsW = w;
        d.swsH = h;
    }
    if (!d.sws) return false;

    outFrame.width = w;
    outFrame.height = h;
    outFrame.stride = w * Frame::bytesPerPixel();
    outFrame.format = PixelFormat::BGRA;
    outFrame.data.resize(static_cast<size_t>(outFrame.stride) * h);

    uint8_t* dst[1] = {outFrame.data.data()};
    const int dstStride[1] = {outFrame.stride};
    sws_scale(d.sws, d.yuv->data, d.yuv->linesize, 0, h, dst, dstStride);

    gotFrame = true;
    return true;
}

void H264Decoder::close() { impl_->cleanup(); }

}  // namespace rd
