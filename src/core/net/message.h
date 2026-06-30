#pragma once

#include <cstdint>
#include <vector>

#include "core/net/tcp_socket.h"

namespace rd::net {

// 应用层消息类型。
enum class MsgType : uint8_t {
    // 媒体/控制（host <-> viewer 之间）
    Video = 1,  // payload = H.264 Annex-B 码流
    Input = 2,  // payload = 输入事件（鼠标/键盘）

    // 信令（peer <-> 信令服务器之间），10 及以上
    SigRegister = 10,  // host 注册：payload = 房间/设备 id（UTF-8）
    SigConnect = 11,   // viewer 连接：payload = 要连接的 id（UTF-8）
    SigPaired = 12,    // 服务器通知：配对成功，payload 空
    SigRelay = 13,     // 透传：payload 原样转发给已配对的对端（承载打洞信息）
    SigError = 14,     // 服务器错误：payload = UTF-8 错误描述
};

// 线路格式（大端）：
//   [type: 1 字节][length: 4 字节][payload: length 字节]
//
// 这样 TCP 流就能被切成一条条完整消息，避免粘包/拆包问题。

// 发送一条消息（带分帧头）。
bool sendMessage(TcpConn& conn, MsgType type, const void* payload,
                 uint32_t len);

// 接收一条完整消息。对端关闭或出错返回 false。
bool recvMessage(TcpConn& conn, MsgType& type, std::vector<uint8_t>& payload);

}  // namespace rd::net
