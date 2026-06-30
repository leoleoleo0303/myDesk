#pragma once

#include "core/capture/screen_capture.h"

// 前向声明，避免在头文件里暴露 X11 头（与 <X11/Xlib.h> 中的 typedef 一致）。
typedef struct _XDisplay Display;

namespace rd {

// 基于 X11 的屏幕采集（Linux，X 服务器环境）。
// 当前用 XGetImage 抓全屏，实现简单、稳定；后续实时串流时再换成
// XShm 共享内存以提升性能（见 TODO）。
class X11ScreenCapture : public ScreenCapture {
public:
    ~X11ScreenCapture() override;

    bool init() override;
    bool captureFrame(Frame& out) override;
    void shutdown() override;

private:
    Display* display_ = nullptr;
    unsigned long root_ = 0;  // X11 Window (XID)
    int width_ = 0;
    int height_ = 0;
};

}  // namespace rd
