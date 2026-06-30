// rtc_demo: libdatachannel 冒烟测试。
// 只验证库能正确链接、能构造 PeerConnection 和 DataChannel。
// 不做真实 P2P 连接（那需要信令交换，后续在 host/viewer 里实现）。

#include <cstdio>
#include <memory>

#include <rtc/rtc.hpp>

int main() {
    rtc::InitLogger(rtc::LogLevel::Warning);

    rtc::Configuration config;
    // 公共 STUN 服务器（用于发现公网地址）；正式部署可换成自建
    config.iceServers.emplace_back("stun:stun.l.google.com:19302");

    auto pc = std::make_shared<rtc::PeerConnection>(config);

    pc->onStateChange([](rtc::PeerConnection::State state) {
        std::printf("[rtc] PeerConnection 状态变化\n");
        (void)state;
    });

    auto dc = pc->createDataChannel("smoke-test");
    std::printf("libdatachannel 链接成功，DataChannel label=%s\n",
                dc->label().c_str());

    return 0;
}
