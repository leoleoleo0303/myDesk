#include "core/capture/screen_capture.h"

#if defined(__linux__)
#include "core/capture/capture_x11.h"
#elif defined(_WIN32)
#include "core/capture/capture_win.h"
#elif defined(__APPLE__)
#include "core/capture/capture_mac.h"
#endif

namespace rd {

std::unique_ptr<ScreenCapture> ScreenCapture::create() {
#if defined(__linux__)
    return std::make_unique<X11ScreenCapture>();
#elif defined(_WIN32)
    return std::make_unique<WinScreenCapture>();
#elif defined(__APPLE__)
    return std::make_unique<MacScreenCapture>();
#else
    return nullptr;
#endif
}

}  // namespace rd
