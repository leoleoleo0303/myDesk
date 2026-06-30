#include "core/input/input_event.h"

namespace rd {

void serializeInput(const InputEvent& ev, uint8_t out[kInputEventSize]) {
    out[0] = static_cast<uint8_t>(ev.type);
    out[1] = static_cast<uint8_t>(ev.x >> 8);
    out[2] = static_cast<uint8_t>(ev.x);
    out[3] = static_cast<uint8_t>(ev.y >> 8);
    out[4] = static_cast<uint8_t>(ev.y);
    out[5] = ev.button;
    out[6] = ev.down;
    const uint16_t w = static_cast<uint16_t>(ev.wheel);
    out[7] = static_cast<uint8_t>(w >> 8);
    out[8] = static_cast<uint8_t>(w);
    out[9] = static_cast<uint8_t>(ev.key >> 24);
    out[10] = static_cast<uint8_t>(ev.key >> 16);
    out[11] = static_cast<uint8_t>(ev.key >> 8);
    out[12] = static_cast<uint8_t>(ev.key);
}

bool deserializeInput(const uint8_t* data, size_t len, InputEvent& out) {
    if (len < kInputEventSize) return false;
    out.type = static_cast<InputType>(data[0]);
    out.x = static_cast<uint16_t>((data[1] << 8) | data[2]);
    out.y = static_cast<uint16_t>((data[3] << 8) | data[4]);
    out.button = data[5];
    out.down = data[6];
    out.wheel = static_cast<int16_t>((data[7] << 8) | data[8]);
    out.key = (static_cast<uint32_t>(data[9]) << 24) |
              (static_cast<uint32_t>(data[10]) << 16) |
              (static_cast<uint32_t>(data[11]) << 8) |
              static_cast<uint32_t>(data[12]);
    return true;
}

}  // namespace rd
