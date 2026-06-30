#include "core/capture/capture_x11.h"

#include <cstdio>
#include <cstdlib>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

namespace rd {

namespace {
// 计算颜色掩码的右移位数，例如 0xff0000 -> 16。
int shiftOf(unsigned long mask) {
    int s = 0;
    if (mask == 0) return 0;
    while ((mask & 1ul) == 0) {
        mask >>= 1;
        ++s;
    }
    return s;
}
}  // namespace

X11ScreenCapture::~X11ScreenCapture() { shutdown(); }

bool X11ScreenCapture::init() {
    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        const char* disp = std::getenv("DISPLAY");
        const char* sess = std::getenv("XDG_SESSION_TYPE");
        std::fprintf(stderr,
                     "[capture] XOpenDisplay 失败: 连不上 X 服务器\n"
                     "  DISPLAY=%s  XDG_SESSION_TYPE=%s\n"
                     "  常见原因: 1) 用了 sudo(会丢 DISPLAY/XAUTHORITY)\n"
                     "           2) Wayland 会话(X 截屏被禁, 需 PipeWire 采集,\n"
                     "              或注销后选 'Ubuntu on Xorg' 登录)\n"
                     "           3) SSH 无图形会话\n",
                     disp ? disp : "(空)", sess ? sess : "(空)");
        return false;
    }
    const int screen = DefaultScreen(display_);
    root_ = RootWindow(display_, screen);
    width_ = DisplayWidth(display_, screen);
    height_ = DisplayHeight(display_, screen);
    if (width_ <= 0 || height_ <= 0) {
        std::fprintf(stderr, "[capture] 屏幕尺寸异常: %dx%d\n", width_, height_);
        return false;
    }
    return true;
}

bool X11ScreenCapture::captureFrame(Frame& out) {
    if (!display_) return false;

    XImage* img = XGetImage(display_, root_, 0, 0,
                            width_, height_, AllPlanes, ZPixmap);
    if (!img) return false;

    out.width = width_;
    out.height = height_;
    out.stride = width_ * Frame::bytesPerPixel();
    out.format = PixelFormat::BGRA;
    out.data.resize(static_cast<size_t>(out.stride) * height_);

    const int rshift = shiftOf(img->red_mask);
    const int gshift = shiftOf(img->green_mask);
    const int bshift = shiftOf(img->blue_mask);

    uint8_t* dst = out.data.data();
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const unsigned long pixel = XGetPixel(img, x, y);
            const uint8_t r =
                static_cast<uint8_t>((pixel & img->red_mask) >> rshift);
            const uint8_t g =
                static_cast<uint8_t>((pixel & img->green_mask) >> gshift);
            const uint8_t b =
                static_cast<uint8_t>((pixel & img->blue_mask) >> bshift);

            const size_t idx =
                static_cast<size_t>(y) * out.stride + static_cast<size_t>(x) * 4;
            dst[idx + 0] = b;
            dst[idx + 1] = g;
            dst[idx + 2] = r;
            dst[idx + 3] = 255;
        }
    }

    XDestroyImage(img);
    return true;
}

void X11ScreenCapture::shutdown() {
    if (display_) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}

}  // namespace rd
