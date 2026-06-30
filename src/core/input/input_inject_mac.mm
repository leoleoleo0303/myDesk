#if defined(__APPLE__)

#include "core/input/input_inject_mac.h"
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <unordered_map>
#include <memory>

namespace rd {

// ---------------------------------------------------------------------------
// X11 keysym to macOS virtual keycode mapping
// ---------------------------------------------------------------------------
static const std::unordered_map<uint32_t, CGKeyCode>& keysymToKeycode() {
    static const std::unordered_map<uint32_t, CGKeyCode> map = {
        // Letters (XK_a..XK_z = 0x0061..0x007a, XK_A..XK_Z = 0x0041..0x005a)
        {0x0061, kVK_ANSI_A}, {0x0062, kVK_ANSI_B}, {0x0063, kVK_ANSI_C},
        {0x0064, kVK_ANSI_D}, {0x0065, kVK_ANSI_E}, {0x0066, kVK_ANSI_F},
        {0x0067, kVK_ANSI_G}, {0x0068, kVK_ANSI_H}, {0x0069, kVK_ANSI_I},
        {0x006a, kVK_ANSI_J}, {0x006b, kVK_ANSI_K}, {0x006c, kVK_ANSI_L},
        {0x006d, kVK_ANSI_M}, {0x006e, kVK_ANSI_N}, {0x006f, kVK_ANSI_O},
        {0x0070, kVK_ANSI_P}, {0x0071, kVK_ANSI_Q}, {0x0072, kVK_ANSI_R},
        {0x0073, kVK_ANSI_S}, {0x0074, kVK_ANSI_T}, {0x0075, kVK_ANSI_U},
        {0x0076, kVK_ANSI_V}, {0x0077, kVK_ANSI_W}, {0x0078, kVK_ANSI_X},
        {0x0079, kVK_ANSI_Y}, {0x007a, kVK_ANSI_Z},
        // Uppercase maps to same keycodes
        {0x0041, kVK_ANSI_A}, {0x0042, kVK_ANSI_B}, {0x0043, kVK_ANSI_C},
        {0x0044, kVK_ANSI_D}, {0x0045, kVK_ANSI_E}, {0x0046, kVK_ANSI_F},
        {0x0047, kVK_ANSI_G}, {0x0048, kVK_ANSI_H}, {0x0049, kVK_ANSI_I},
        {0x004a, kVK_ANSI_J}, {0x004b, kVK_ANSI_K}, {0x004c, kVK_ANSI_L},
        {0x004d, kVK_ANSI_M}, {0x004e, kVK_ANSI_N}, {0x004f, kVK_ANSI_O},
        {0x0050, kVK_ANSI_P}, {0x0051, kVK_ANSI_Q}, {0x0052, kVK_ANSI_R},
        {0x0053, kVK_ANSI_S}, {0x0054, kVK_ANSI_T}, {0x0055, kVK_ANSI_U},
        {0x0056, kVK_ANSI_V}, {0x0057, kVK_ANSI_W}, {0x0058, kVK_ANSI_X},
        {0x0059, kVK_ANSI_Y}, {0x005a, kVK_ANSI_Z},
        // Numbers (XK_0..XK_9 = 0x0030..0x0039)
        {0x0030, kVK_ANSI_0}, {0x0031, kVK_ANSI_1}, {0x0032, kVK_ANSI_2},
        {0x0033, kVK_ANSI_3}, {0x0034, kVK_ANSI_4}, {0x0035, kVK_ANSI_5},
        {0x0036, kVK_ANSI_6}, {0x0037, kVK_ANSI_7}, {0x0038, kVK_ANSI_8},
        {0x0039, kVK_ANSI_9},
        // Function keys (XK_F1..XK_F12 = 0xFFBE..0xFFC9)
        {0xFFBE, kVK_F1},  {0xFFBF, kVK_F2},  {0xFFC0, kVK_F3},
        {0xFFC1, kVK_F4},  {0xFFC2, kVK_F5},  {0xFFC3, kVK_F6},
        {0xFFC4, kVK_F7},  {0xFFC5, kVK_F8},  {0xFFC6, kVK_F9},
        {0xFFC7, kVK_F10}, {0xFFC8, kVK_F11}, {0xFFC9, kVK_F12},
        // Modifiers
        {0xFFE1, kVK_Shift},       // XK_Shift_L
        {0xFFE2, kVK_RightShift},  // XK_Shift_R
        {0xFFE3, kVK_Control},     // XK_Control_L
        {0xFFE4, kVK_RightControl},// XK_Control_R
        {0xFFE9, kVK_Option},      // XK_Alt_L
        {0xFFEA, kVK_RightOption}, // XK_Alt_R
        {0xFFE7, kVK_Command},     // XK_Meta_L
        {0xFFE8, kVK_RightCommand},// XK_Meta_R
        {0xFFEB, kVK_Command},     // XK_Super_L (map to Cmd)
        {0xFFEC, kVK_RightCommand},// XK_Super_R
        // Navigation
        {0xFF0D, kVK_Return},      // XK_Return
        {0xFF1B, kVK_Escape},      // XK_Escape
        {0xFF08, kVK_Delete},      // XK_BackSpace
        {0xFFFF, kVK_ForwardDelete},// XK_Delete
        {0xFF09, kVK_Tab},         // XK_Tab
        {0x0020, kVK_Space},       // XK_space
        // Arrow keys
        {0xFF51, kVK_LeftArrow},   // XK_Left
        {0xFF52, kVK_UpArrow},     // XK_Up
        {0xFF53, kVK_RightArrow},  // XK_Right
        {0xFF54, kVK_DownArrow},   // XK_Down
        // Page navigation
        {0xFF55, kVK_PageUp},      // XK_Page_Up
        {0xFF56, kVK_PageDown},    // XK_Page_Down
        {0xFF50, kVK_Home},        // XK_Home
        {0xFF57, kVK_End},         // XK_End
        // Misc
        {0xFFE5, kVK_CapsLock},    // XK_Caps_Lock
        // Punctuation / symbols
        {0x002d, kVK_ANSI_Minus},        // -
        {0x003d, kVK_ANSI_Equal},        // =
        {0x005b, kVK_ANSI_LeftBracket},  // [
        {0x005d, kVK_ANSI_RightBracket}, // ]
        {0x005c, kVK_ANSI_Backslash},    // backslash
        {0x003b, kVK_ANSI_Semicolon},    // ;
        {0x0027, kVK_ANSI_Quote},        // '
        {0x002c, kVK_ANSI_Comma},        // ,
        {0x002e, kVK_ANSI_Period},       // .
        {0x002f, kVK_ANSI_Slash},        // /
        {0x0060, kVK_ANSI_Grave},        // `
    };
    return map;
}

// ---------------------------------------------------------------------------
// Convert X11 keysym to macOS virtual keycode
// Returns -1 if not found
// ---------------------------------------------------------------------------
static CGKeyCode keysymToVirtualKey(uint32_t keysym) {
    const auto& map = keysymToKeycode();
    auto it = map.find(keysym);
    if (it != map.end()) {
        return it->second;
    }
    return static_cast<CGKeyCode>(0xFFFF); // invalid
}

// ---------------------------------------------------------------------------
// MacInputInjector implementation
// ---------------------------------------------------------------------------

bool MacInputInjector::init(int screenW, int screenH) {
    if (screenW <= 0 || screenH <= 0) {
        return false;
    }
    screenW_ = screenW;
    screenH_ = screenH;
    return true;
}

void MacInputInjector::inject(const InputEvent& ev) {
    switch (ev.type) {
        case InputType::MouseMove: {
            // Convert normalized coords (0..65535) to screen coords
            CGFloat px = static_cast<CGFloat>(ev.x) / 65535.0 * screenW_;
            CGFloat py = static_cast<CGFloat>(ev.y) / 65535.0 * screenH_;

            CGEventRef event = CGEventCreateMouseEvent(
                nullptr, kCGEventMouseMoved, CGPointMake(px, py), kCGMouseButtonLeft);
            if (event) {
                CGEventPost(kCGHIDEventTap, event);
                CFRelease(event);
            }
            break;
        }

        case InputType::MouseButton: {
            CGFloat px = static_cast<CGFloat>(ev.x) / 65535.0 * screenW_;
            CGFloat py = static_cast<CGFloat>(ev.y) / 65535.0 * screenH_;

            CGEventType eventType;
            CGMouseButton cgButton;

            switch (ev.button) {
                case 1: // left
                    cgButton = kCGMouseButtonLeft;
                    eventType = ev.down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
                    break;
                case 2: // middle
                    cgButton = kCGMouseButtonCenter;
                    eventType = ev.down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
                    break;
                case 3: // right
                    cgButton = kCGMouseButtonRight;
                    eventType = ev.down ? kCGEventRightMouseDown : kCGEventRightMouseUp;
                    break;
                default:
                    return;
            }

            CGEventRef event = CGEventCreateMouseEvent(
                nullptr, eventType, CGPointMake(px, py), cgButton);
            if (event) {
                CGEventPost(kCGHIDEventTap, event);
                CFRelease(event);
            }
            break;
        }

        case InputType::MouseWheel: {
            // ev.wheel: positive = scroll up, negative = scroll down
            CGEventRef event = CGEventCreateScrollWheelEvent(
                nullptr, kCGScrollEventUnitLine, 1, static_cast<int32_t>(ev.wheel));
            if (event) {
                CGEventPost(kCGHIDEventTap, event);
                CFRelease(event);
            }
            break;
        }

        case InputType::Key: {
            CGKeyCode keycode = keysymToVirtualKey(ev.key);
            if (keycode == static_cast<CGKeyCode>(0xFFFF)) {
                return; // unmapped key
            }

            bool keyDown = (ev.down != 0);
            CGEventRef event = CGEventCreateKeyboardEvent(nullptr, keycode, keyDown);
            if (event) {
                CGEventPost(kCGHIDEventTap, event);
                CFRelease(event);
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Factory method
// ---------------------------------------------------------------------------
std::unique_ptr<InputInjector> InputInjector::create() {
    return std::make_unique<MacInputInjector>();
}

} // namespace rd

#endif // defined(__APPLE__)
