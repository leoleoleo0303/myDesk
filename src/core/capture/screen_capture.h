#pragma once

#include <memory>

#include "core/common/frame.h"

namespace rd {

// 屏幕采集抽象接口。每个平台（X11 / Windows / macOS ...）实现一份。
class ScreenCapture {
public:
    virtual ~ScreenCapture() = default;

    // 初始化：打开显示设备、查询分辨率、分配资源。失败返回 false。
    virtual bool init() = 0;

    // 采集一帧。成功返回 true，画面写入 out（BGRA）。
    virtual bool captureFrame(Frame& out) = 0;

    // 释放资源。
    virtual void shutdown() = 0;

    // 工厂：根据当前编译平台创建对应实现。无可用实现返回 nullptr。
    static std::unique_ptr<ScreenCapture> create();
};

}  // namespace rd
