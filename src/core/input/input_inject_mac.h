#pragma once
#include "core/input/input_injector.h"

namespace rd {

class MacInputInjector : public InputInjector {
public:
    ~MacInputInjector() override = default;
    bool init(int screenW, int screenH) override;
    void inject(const InputEvent& ev) override;

private:
    int screenW_ = 0;
    int screenH_ = 0;
};

} // namespace rd
