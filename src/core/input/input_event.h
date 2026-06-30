#pragma once

#include <cstddef>
#include <cstdint>

namespace rd {

// 跨平台输入事件。坐标用归一化值（0..65535）相对远端屏幕，
// 这样控制端与被控端分辨率不同也能正确映射。
// 键值 key 采用 X11 keysym 语义（可打印 ASCII 与其码点一致），
// 各平台注入器再翻译成本地键码。
enum class InputType : uint8_t {
    MouseMove = 1,    // x, y
    MouseButton = 2,  // button, down
    MouseWheel = 3,   // wheel (+上 / -下)
    Key = 4,          // key, down
};

struct InputEvent {
    InputType type = InputType::MouseMove;
    uint16_t x = 0;       // 归一化横坐标 0..65535
    uint16_t y = 0;       // 归一化纵坐标 0..65535
    uint8_t button = 0;   // 1=左 2=中 3=右（X11 按键编号约定）
    uint8_t down = 0;     // 1=按下 0=抬起
    int16_t wheel = 0;    // 滚轮步数
    uint32_t key = 0;     // X11-style keysym
};

// 固定 13 字节的线路格式（大端）。
constexpr size_t kInputEventSize = 13;

void serializeInput(const InputEvent& ev, uint8_t out[kInputEventSize]);
bool deserializeInput(const uint8_t* data, size_t len, InputEvent& out);

}  // namespace rd
