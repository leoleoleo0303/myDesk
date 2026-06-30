#include "core/capture/capture_win.h"

#if defined(_WIN32)

#include <windows.h>

namespace rd {

WinScreenCapture::~WinScreenCapture() { shutdown(); }

bool WinScreenCapture::init() {
    return true;  // GDI 无需显式初始化
}

bool WinScreenCapture::captureFrame(Frame& out) {
    const int width = GetSystemMetrics(SM_CXSCREEN);
    const int height = GetSystemMetrics(SM_CYSCREEN);
    if (width <= 0 || height <= 0) return false;

    HDC hScreen = GetDC(nullptr);
    HDC hMemDC = CreateCompatibleDC(hScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, width, height);
    HGDIOBJ oldObj = SelectObject(hMemDC, hBitmap);

    BitBlt(hMemDC, 0, 0, width, height, hScreen, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi{};
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height;  // 负数 = top-down，行序与我们的 Frame 一致
    bi.biPlanes = 1;
    bi.biBitCount = 32;     // 32 位 DIB 字节序为 BGRA，正好匹配我们的格式
    bi.biCompression = BI_RGB;

    out.width = width;
    out.height = height;
    out.stride = width * Frame::bytesPerPixel();
    out.format = PixelFormat::BGRA;
    out.data.resize(static_cast<size_t>(out.stride) * height);

    const int scanned = GetDIBits(hMemDC, hBitmap, 0, height, out.data.data(),
                                  reinterpret_cast<BITMAPINFO*>(&bi),
                                  DIB_RGB_COLORS);

    SelectObject(hMemDC, oldObj);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreen);

    return scanned != 0;
}

void WinScreenCapture::shutdown() {}

}  // namespace rd

#endif  // _WIN32
