#include "core/capture/screen_recorder.h"

#include "core/capture/screen_capture.h"
#include "core/common/frame.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <chrono>
#include <cstdio>

namespace rd {

ScreenRecorder::ScreenRecorder() = default;

ScreenRecorder::~ScreenRecorder() { stop(); }

bool ScreenRecorder::start(const std::string& outputPath, const Config& cfg) {
    if (recording_.load()) return false;

    recording_.store(true);
    frameCount_.store(0);
    fps_ = cfg.fps;

    thread_ = std::thread([this, outputPath, cfg]() {
        recordLoop(outputPath, cfg);
    });

    return true;
}

void ScreenRecorder::stop() {
    recording_.store(false);
    if (thread_.joinable()) thread_.join();
}

double ScreenRecorder::durationSeconds() const {
    const int64_t frames = frameCount_.load();
    if (fps_ <= 0) return 0.0;
    return static_cast<double>(frames) / fps_;
}

void ScreenRecorder::recordLoop(const std::string& outputPath, Config cfg) {
    // 1. 初始化屏幕采集
    auto cap = ScreenCapture::create();
    if (!cap || !cap->init()) {
        if (errorCb_) errorCb_("Failed to init screen capture");
        recording_.store(false);
        return;
    }

    // 采集第一帧确定分辨率
    Frame frame;
    if (!cap->captureFrame(frame) || frame.empty()) {
        if (errorCb_) errorCb_("Failed to capture initial frame");
        cap->shutdown();
        recording_.store(false);
        return;
    }

    const int width = frame.width;
    const int height = frame.height;

    // 2. 初始化 FFmpeg muxer（MP4）
    AVFormatContext* fmtCtx = nullptr;
    if (avformat_alloc_output_context2(&fmtCtx, nullptr, nullptr,
                                       outputPath.c_str()) < 0) {
        if (errorCb_) errorCb_("Failed to create output format context");
        cap->shutdown();
        recording_.store(false);
        return;
    }

    // 3. 查找 H.264 编码器并创建流
    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        if (errorCb_) errorCb_("H.264 encoder not found");
        avformat_free_context(fmtCtx);
        cap->shutdown();
        recording_.store(false);
        return;
    }

    AVStream* stream = avformat_new_stream(fmtCtx, codec);
    if (!stream) {
        if (errorCb_) errorCb_("Failed to create video stream");
        avformat_free_context(fmtCtx);
        cap->shutdown();
        recording_.store(false);
        return;
    }

    AVCodecContext* encCtx = avcodec_alloc_context3(codec);
    if (!encCtx) {
        if (errorCb_) errorCb_("Failed to allocate encoder context");
        avformat_free_context(fmtCtx);
        cap->shutdown();
        recording_.store(false);
        return;
    }

    encCtx->width = width;
    encCtx->height = height;
    encCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    encCtx->time_base = AVRational{1, cfg.fps};
    encCtx->framerate = AVRational{cfg.fps, 1};
    encCtx->bit_rate = static_cast<int64_t>(cfg.bitrateKbps) * 1000;
    encCtx->gop_size = cfg.gopSize;
    encCtx->max_b_frames = 0;

    av_opt_set(encCtx->priv_data, "preset", "veryfast", 0);
    av_opt_set(encCtx->priv_data, "tune", "zerolatency", 0);

    // MP4 需要全局头
    if (fmtCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        encCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(encCtx, codec, nullptr) < 0) {
        if (errorCb_) errorCb_("Failed to open encoder");
        avcodec_free_context(&encCtx);
        avformat_free_context(fmtCtx);
        cap->shutdown();
        recording_.store(false);
        return;
    }

    // 复制编码器参数到流
    avcodec_parameters_from_context(stream->codecpar, encCtx);
    stream->time_base = encCtx->time_base;

    // 4. 打开输出文件
    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmtCtx->pb, outputPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            if (errorCb_) errorCb_("Failed to open output file: " + outputPath);
            avcodec_free_context(&encCtx);
            avformat_free_context(fmtCtx);
            cap->shutdown();
            recording_.store(false);
            return;
        }
    }

    // 写文件头
    if (avformat_write_header(fmtCtx, nullptr) < 0) {
        if (errorCb_) errorCb_("Failed to write file header");
        avcodec_free_context(&encCtx);
        if (!(fmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx);
        cap->shutdown();
        recording_.store(false);
        return;
    }

    // 5. 准备色彩空间转换 (BGRA -> YUV420P)
    SwsContext* sws = sws_getContext(width, height, AV_PIX_FMT_BGRA,
                                     width, height, AV_PIX_FMT_YUV420P,
                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws) {
        if (errorCb_) errorCb_("Failed to create sws context");
        av_write_trailer(fmtCtx);
        avcodec_free_context(&encCtx);
        if (!(fmtCtx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&fmtCtx->pb);
        avformat_free_context(fmtCtx);
        cap->shutdown();
        recording_.store(false);
        return;
    }

    AVFrame* yuv = av_frame_alloc();
    yuv->format = AV_PIX_FMT_YUV420P;
    yuv->width = width;
    yuv->height = height;
    av_frame_get_buffer(yuv, 32);

    AVPacket* pkt = av_packet_alloc();

    // 6. 录制循环
    const auto interval = std::chrono::microseconds(1000000 / cfg.fps);
    int64_t pts = 0;

    while (recording_.load()) {
        const auto t0 = std::chrono::steady_clock::now();

        if (!cap->captureFrame(frame) || frame.empty()) {
            // 采集失败跳过一帧
            std::this_thread::sleep_for(interval);
            continue;
        }

        // BGRA -> YUV420P
        av_frame_make_writable(yuv);
        const uint8_t* srcSlice[1] = {frame.data.data()};
        const int srcStride[1] = {frame.stride};
        sws_scale(sws, srcSlice, srcStride, 0, height, yuv->data, yuv->linesize);

        yuv->pts = pts++;

        // 编码
        if (avcodec_send_frame(encCtx, yuv) == 0) {
            while (avcodec_receive_packet(encCtx, pkt) == 0) {
                av_packet_rescale_ts(pkt, encCtx->time_base, stream->time_base);
                pkt->stream_index = stream->index;
                av_interleaved_write_frame(fmtCtx, pkt);
                av_packet_unref(pkt);
            }
        }

        frameCount_.store(pts);

        // 控制帧率
        const auto elapsed = std::chrono::steady_clock::now() - t0;
        if (elapsed < interval) {
            std::this_thread::sleep_for(interval - elapsed);
        }
    }

    // 7. 冲洗编码器
    avcodec_send_frame(encCtx, nullptr);
    while (avcodec_receive_packet(encCtx, pkt) == 0) {
        av_packet_rescale_ts(pkt, encCtx->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        av_interleaved_write_frame(fmtCtx, pkt);
        av_packet_unref(pkt);
    }

    // 8. 清理
    av_write_trailer(fmtCtx);

    av_packet_free(&pkt);
    av_frame_free(&yuv);
    sws_freeContext(sws);
    avcodec_free_context(&encCtx);
    if (!(fmtCtx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&fmtCtx->pb);
    avformat_free_context(fmtCtx);
    cap->shutdown();

    recording_.store(false);
}

}  // namespace rd
