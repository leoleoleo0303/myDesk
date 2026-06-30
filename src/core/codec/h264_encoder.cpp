#include "core/codec/h264_encoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace rd {

struct H264Encoder::Impl {
    const AVCodec* codec = nullptr;
    AVCodecContext* ctx = nullptr;
    AVFrame* yuv = nullptr;     // 转换后的 YUV420P 帧
    AVPacket* pkt = nullptr;
    SwsContext* sws = nullptr;  // BGRA -> YUV420P
    int width = 0;
    int height = 0;
    int64_t pts = 0;

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

    // 把编码器吐出的包搬运到 out。
    bool drain(std::vector<EncodedPacket>& out) {
        for (;;) {
            const int ret = avcodec_receive_packet(ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return true;
            if (ret < 0) return false;

            EncodedPacket ep;
            ep.data.assign(pkt->data, pkt->data + pkt->size);
            ep.keyframe = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
            ep.pts = pkt->pts;
            out.push_back(std::move(ep));

            av_packet_unref(pkt);
        }
    }
};

H264Encoder::H264Encoder() : impl_(std::make_unique<Impl>()) {}
H264Encoder::~H264Encoder() = default;

bool H264Encoder::open(const Config& cfg) {
    if (cfg.width <= 0 || cfg.height <= 0) return false;

    auto& d = *impl_;
    d.width = cfg.width;
    d.height = cfg.height;

    d.codec = avcodec_find_encoder_by_name("libx264");
    if (!d.codec) d.codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!d.codec) return false;

    d.ctx = avcodec_alloc_context3(d.codec);
    if (!d.ctx) return false;

    d.ctx->width = cfg.width;
    d.ctx->height = cfg.height;
    d.ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    d.ctx->time_base = AVRational{1, cfg.fps};
    d.ctx->framerate = AVRational{cfg.fps, 1};
    d.ctx->bit_rate = static_cast<int64_t>(cfg.bitrateKbps) * 1000;
    d.ctx->gop_size = cfg.gopSize;
    d.ctx->max_b_frames = 0;  // 低延迟：不用 B 帧

    // libx264 低延迟实时调优
    av_opt_set(d.ctx->priv_data, "preset", "veryfast", 0);
    av_opt_set(d.ctx->priv_data, "tune", "zerolatency", 0);

    if (avcodec_open2(d.ctx, d.codec, nullptr) < 0) return false;

    d.yuv = av_frame_alloc();
    if (!d.yuv) return false;
    d.yuv->format = AV_PIX_FMT_YUV420P;
    d.yuv->width = cfg.width;
    d.yuv->height = cfg.height;
    if (av_frame_get_buffer(d.yuv, 32) < 0) return false;

    d.pkt = av_packet_alloc();
    if (!d.pkt) return false;

    d.sws = sws_getContext(cfg.width, cfg.height, AV_PIX_FMT_BGRA,
                           cfg.width, cfg.height, AV_PIX_FMT_YUV420P,
                           SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!d.sws) return false;

    return true;
}

bool H264Encoder::encode(const Frame& frame, std::vector<EncodedPacket>& out) {
    auto& d = *impl_;
    if (!d.ctx) return false;
    if (frame.width != d.width || frame.height != d.height) return false;

    if (av_frame_make_writable(d.yuv) < 0) return false;

    // BGRA -> YUV420P
    const uint8_t* srcSlice[1] = {frame.data.data()};
    const int srcStride[1] = {frame.stride};
    sws_scale(d.sws, srcSlice, srcStride, 0, d.height, d.yuv->data,
              d.yuv->linesize);

    d.yuv->pts = d.pts++;

    if (avcodec_send_frame(d.ctx, d.yuv) < 0) return false;
    return d.drain(out);
}

bool H264Encoder::flush(std::vector<EncodedPacket>& out) {
    auto& d = *impl_;
    if (!d.ctx) return false;
    if (avcodec_send_frame(d.ctx, nullptr) < 0) return false;  // 进入冲洗
    return d.drain(out);
}

void H264Encoder::close() { impl_->cleanup(); }

}  // namespace rd
