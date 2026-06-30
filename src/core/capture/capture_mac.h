#pragma once
#include "core/capture/screen_capture.h"

namespace rd {

class MacScreenCapture : public ScreenCapture {
public:
    ~MacScreenCapture() override;
    bool init() override;
    bool captureFrame(Frame& out) override;
    void shutdown() override;
};

} // namespace rd
