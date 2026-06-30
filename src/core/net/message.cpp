#include "core/net/message.h"

namespace rd::net {

namespace {
constexpr uint32_t kMaxPayload = 64u * 1024 * 1024;  // 64MB 上限，防异常

void writeBE32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v >> 24);
    p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >> 8);
    p[3] = static_cast<uint8_t>(v);
}

uint32_t readBE32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}
}  // namespace

bool sendMessage(TcpConn& conn, MsgType type, const void* payload,
                 uint32_t len) {
    uint8_t header[5];
    header[0] = static_cast<uint8_t>(type);
    writeBE32(header + 1, len);
    if (!conn.sendAll(header, sizeof(header))) return false;
    if (len == 0) return true;
    return conn.sendAll(payload, len);
}

bool recvMessage(TcpConn& conn, MsgType& type, std::vector<uint8_t>& payload) {
    uint8_t header[5];
    if (!conn.recvAll(header, sizeof(header))) return false;

    type = static_cast<MsgType>(header[0]);
    const uint32_t len = readBE32(header + 1);
    if (len > kMaxPayload) return false;

    payload.resize(len);
    if (len == 0) return true;
    return conn.recvAll(payload.data(), len);
}

}  // namespace rd::net
