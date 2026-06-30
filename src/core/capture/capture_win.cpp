#include "core/capture/capture_win.h"

#if defined(_WIN32)

#include <windows.h>

namespace rd {

WinScreenCapture::~WinScreenCapture() { shutdown(); }

bool WinScreenCapture::init() {
    // 预先获取屏幕尺寸，创建复用的 DC 和 bitmap，避免每帧都创建/销毁
    width_ = GetSystemMetrics(SM_CXSCREEN);
    height_ = GetSystemMetrics(SM_CYSCREEN);
    if (width_ <= 0 || height_ <= 0) return false;

    hScreenDC_ = GetDC(nullptr);
    if (!hScreenDC_) return false;

    hMemDC_ = CreateCompatibleDC(hScreenDC_);
    if (!hMemDC_) return false;

    hBitmap_ = CreateCompatibleBitmap(hScreenDC_, width_, height_);
    if (!hBitmap_) return false;

    hOldObj_ = SelectObject(hMemDC_, hBitmap_);

    bi_ = {};
    bi_.biSize = sizeof(BITMAPINFOHEADER);
    bi_.biWidth = width_;
    bi_.biHeight = -height_;  // top-down
    bi_.biPlanes = 1;
    bi_.biBitCount = 32;
    bi_.biCompression = BI_RGB;

    // 预分配帧缓冲
    bufferSize_ = static_cast<size_t>(width_) * height_ * 4;

    return true;
}

bool WinScreenCapture::captureFrame(Frame& out) {
    if (!hMemDC_) return false;

    // 检查分辨率是否变化
    const int curW = GetSystemMetrics(SM_CXSCREEN);
    const int curH = GetSystemMetrics(SM_CYSCREEN);
    if (curW != width_ || curH != height_) {
        shutdown();
        if (!init()) return false;
    }

    BitBlt(hMemDC_, 0, 0, width_, height_, hScreenDC_, 0, 0, SRCCOPY);

    out.width = width_;
    out.height = height_;
    out.stride = width_ * Frame::bytesPerPixel();
    out.format = PixelFormat::BGRA;

    // 复用已有的 buffer，避免每帧 malloc
    if (out.data.size() != bufferSize_) {
        out.data.resize(bufferSize_);
    }

    const int scanned = GetDIBits(hMemDC_, hBitmap_, 0, height_,
                                  out.data.data(),
                                  reinterpret_cast<BITMAPINFO*>(&bi_),
                                  DIB_RGB_COLORS);
    return scanned != 0;
}

void WinScreenCapture::shutdown() {
    if (hOldObj_ && hMemDC_) {
        SelectObject(hMemDC_, hOldObj_);
        hOldObj_ = nullptr;
    }
    if (hBitmap_) {
        DeleteObject(hBitmap_);
        hBitmap_ = nullptr;
    }
    if (hMemDC_) {
        DeleteDC(hMemDC_);
        hMemDC_ = nullptr;
    }
    if (hScreenDC_) {
        ReleaseDC(nullptr, hScreenDC_);
        hScreenDC_ = nullptr;
    }
}

}  // namespace rd

#endif  // _WIN32
