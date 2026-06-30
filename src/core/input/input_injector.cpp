#include "core/input/input_injector.h"

#if defined(__linux__)
#include "core/input/input_inject_x11.h"
#elif defined(_WIN32)
#include "core/input/input_inject_win.h"
#elif defined(__APPLE__)
#include "core/input/input_inject_mac.h"
#endif

namespace rd {

std::unique_ptr<InputInjector> InputInjector::create() {
#if defined(__linux__)
    return std::make_unique<X11InputInjector>();
#elif defined(_WIN32)
    return std::make_unique<WinInputInjector>();
#elif defined(__APPLE__)
    return std::make_unique<MacInputInjector>();
#else
    return nullptr;
#endif
}

}  // namespace rd
