#pragma once

#include "core/capture/screen_capture.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace rd {

// 基于 GDI BitBlt 的屏幕采集（Windows）。
// 资源在 init() 时预分配，captureFrame() 复用，减少每帧开销。
// 后续可换成 DXGI Desktop Duplication 以获得更高性能。
class WinScreenCapture : public ScreenCapture {
public:
    ~WinScreenCapture() override;

    bool init() override;
    bool captureFrame(Frame& out) override;
    void shutdown() override;

private:
#if defined(_WIN32)
    HDC hScreenDC_ = nullptr;
    HDC hMemDC_ = nullptr;
    HBITMAP hBitmap_ = nullptr;
    HGDIOBJ hOldObj_ = nullptr;
    BITMAPINFOHEADER bi_{};
    int width_ = 0;
    int height_ = 0;
    size_t bufferSize_ = 0;
#endif
};

}  // namespace rd
