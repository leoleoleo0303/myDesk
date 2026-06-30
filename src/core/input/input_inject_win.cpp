#include "core/input/input_inject_win.h"

#if defined(_WIN32)

#include <windows.h>

namespace rd {

namespace {

// 把 X11 keysym 翻译成 Windows 虚拟键码（VK）。
// 可打印 ASCII 用 VkKeyScan，特殊键查表。返回 0 表示无法映射。
WORD keysymToVK(uint32_t keysym) {
    // 常用特殊键
    switch (keysym) {
        case 0xff08: return VK_BACK;
        case 0xff09: return VK_TAB;
        case 0xff0d: return VK_RETURN;
        case 0xff1b: return VK_ESCAPE;
        case 0xffff: return VK_DELETE;
        case 0xff50: return VK_HOME;
        case 0xff51: return VK_LEFT;
        case 0xff52: return VK_UP;
        case 0xff53: return VK_RIGHT;
        case 0xff54: return VK_DOWN;
        case 0xff55: return VK_PRIOR;
        case 0xff56: return VK_NEXT;
        case 0xff57: return VK_END;
        case 0xff63: return VK_INSERT;
        case 0xffe1: return VK_LSHIFT;
        case 0xffe2: return VK_RSHIFT;
        case 0xffe3: return VK_LCONTROL;
        case 0xffe4: return VK_RCONTROL;
        case 0xffe5: return VK_CAPITAL;
        case 0xffe9: return VK_LMENU;
        case 0xffea: return VK_RMENU;
        case 0x20:   return VK_SPACE;
        default: break;
    }
    // F1 ~ F12: keysym 0xffbe ~ 0xffc9
    if (keysym >= 0xffbe && keysym <= 0xffc9) {
        return static_cast<WORD>(VK_F1 + (keysym - 0xffbe));
    }
    // 可打印 ASCII
    if (keysym >= 0x20 && keysym <= 0x7e) {
        const SHORT s = VkKeyScanA(static_cast<char>(keysym));
        if (s != -1) return static_cast<WORD>(s & 0xff);
    }
    return 0;
}

void sendMouse(DWORD flags, LONG x = 0, LONG y = 0, DWORD data = 0) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = x;
    in.mi.dy = y;
    in.mi.mouseData = data;
    in.mi.dwFlags = flags | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &in, sizeof(INPUT));
}

}  // namespace

bool WinInputInjector::init(int screenW, int screenH) {
    screenW_ = screenW;
    screenH_ = screenH;
    return true;
}

void WinInputInjector::inject(const InputEvent& ev) {
    switch (ev.type) {
        case InputType::MouseMove: {
            // 绝对坐标：MOUSEEVENTF_ABSOLUTE 使用 0..65535 映射到主屏，
            // 与我们的归一化坐标一致，直接传。
            sendMouse(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE, ev.x, ev.y);
            break;
        }
        case InputType::MouseButton: {
            DWORD flags = 0;
            switch (ev.button) {
                case 1: flags = ev.down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
                case 2: flags = ev.down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
                case 3: flags = ev.down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
                default: return;
            }
            sendMouse(flags);
            break;
        }
        case InputType::MouseWheel: {
            sendMouse(MOUSEEVENTF_WHEEL, 0, 0,
                      static_cast<DWORD>(ev.wheel * WHEEL_DELTA));
            break;
        }
        case InputType::Key: {
            const WORD vk = keysymToVK(ev.key);
            if (vk == 0) return;
            INPUT in{};
            in.type = INPUT_KEYBOARD;
            in.ki.wVk = vk;
            in.ki.dwFlags = ev.down ? 0 : KEYEVENTF_KEYUP;
            SendInput(1, &in, sizeof(INPUT));
            break;
        }
    }
}

}  // namespace rd

#endif  // _WIN32
