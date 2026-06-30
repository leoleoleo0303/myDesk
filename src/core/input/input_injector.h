#pragma once

#include <memory>

#include "core/input/input_event.h"

namespace rd {

// 输入注入抽象接口。各平台实现一份（X11 XTest / Windows SendInput）。
class InputInjector {
public:
    virtual ~InputInjector() = default;

    // 初始化。screenW/screenH 为被控端屏幕像素尺寸，用于把归一化坐标
    // 还原成实际像素（部分平台用得到）。
    virtual bool init(int screenW, int screenH) = 0;

    // 注入一个输入事件。
    virtual void inject(const InputEvent& ev) = 0;

    // 工厂：按当前平台创建实现。无可用实现返回 nullptr。
    static std::unique_ptr<InputInjector> create();
};

}  // namespace rd
