#include "core/input/input_inject_x11.h"

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

namespace rd {

X11InputInjector::~X11InputInjector() {
    if (display_) XCloseDisplay(display_);
}

bool X11InputInjector::init(int screenW, int screenH) {
    display_ = XOpenDisplay(nullptr);
    if (!display_) return false;

    int eventBase, errorBase, major, minor;
    if (!XTestQueryExtension(display_, &eventBase, &errorBase, &major, &minor)) {
        XCloseDisplay(display_);
        display_ = nullptr;
        return false;  // X 服务器不支持 XTest
    }

    screenW_ = screenW > 0 ? screenW : DisplayWidth(display_, DefaultScreen(display_));
    screenH_ = screenH > 0 ? screenH : DisplayHeight(display_, DefaultScreen(display_));
    return true;
}

void X11InputInjector::inject(const InputEvent& ev) {
    if (!display_) return;

    switch (ev.type) {
        case InputType::MouseMove: {
            // 归一化 0..65535 -> 像素
            const int px = static_cast<int>(
                static_cast<long>(ev.x) * (screenW_ - 1) / 65535);
            const int py = static_cast<int>(
                static_cast<long>(ev.y) * (screenH_ - 1) / 65535);
            XTestFakeMotionEvent(display_, -1, px, py, 0);
            break;
        }
        case InputType::MouseButton: {
            XTestFakeButtonEvent(display_, ev.button, ev.down ? True : False, 0);
            break;
        }
        case InputType::MouseWheel: {
            // X11 滚轮 = 按键 4(上)/5(下)，每步一次按下+抬起
            const unsigned int btn = ev.wheel > 0 ? 4u : 5u;
            int steps = ev.wheel > 0 ? ev.wheel : -ev.wheel;
            for (int i = 0; i < steps; ++i) {
                XTestFakeButtonEvent(display_, btn, True, 0);
                XTestFakeButtonEvent(display_, btn, False, 0);
            }
            break;
        }
        case InputType::Key: {
            // key 字段即 X11 keysym，直接转 keycode
            const KeyCode kc =
                XKeysymToKeycode(display_, static_cast<KeySym>(ev.key));
            if (kc != 0) {
                XTestFakeKeyEvent(display_, kc, ev.down ? True : False, 0);
            }
            break;
        }
    }
    XFlush(display_);
}

}  // namespace rd
