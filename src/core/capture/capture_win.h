#pragma once

#include "core/capture/screen_capture.h"

namespace rd {

// 基于 GDI BitBlt 的屏幕采集（Windows）。
// 实现简单、兼容性好，适合先跑通链路；后续可换成 DXGI Desktop
// Duplication 以获得更高性能与硬件加速（见 TODO）。
class WinScreenCapture : public ScreenCapture {
public:
    ~WinScreenCapture() override;

    bool init() override;
    bool captureFrame(Frame& out) override;
    void shutdown() override;
};

}  // namespace rd
