#pragma once

#include "core/input/input_injector.h"

namespace rd {

// 基于 SendInput 的输入注入（Windows）。
class WinInputInjector : public InputInjector {
public:
    ~WinInputInjector() override = default;

    bool init(int screenW, int screenH) override;
    void inject(const InputEvent& ev) override;

private:
    int screenW_ = 0;
    int screenH_ = 0;
};

}  // namespace rd
