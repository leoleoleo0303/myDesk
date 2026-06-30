#pragma once

#include "core/input/input_injector.h"

typedef struct _XDisplay Display;

namespace rd {

// 基于 XTest 扩展的输入注入（Linux / X11）。
class X11InputInjector : public InputInjector {
public:
    ~X11InputInjector() override;

    bool init(int screenW, int screenH) override;
    void inject(const InputEvent& ev) override;

private:
    Display* display_ = nullptr;
    int screenW_ = 0;
    int screenH_ = 0;
};

}  // namespace rd
