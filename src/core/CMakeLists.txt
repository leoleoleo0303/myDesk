# rdnet: 仅网络层（TCP + 消息分帧）。独立成库，便于信令服务器在
# 没有 FFmpeg/X11 的服务器上单独构建。
add_library(rdnet
    net/tcp_socket.cpp
    net/message.cpp
)
target_include_directories(rdnet PUBLIC ${CMAKE_SOURCE_DIR}/src)
if(WIN32)
    target_link_libraries(rdnet PUBLIC ws2_32)
endif()

# rdcore: 跨平台远程桌面核心库（采集 + 编解码 + 输入），依赖 rdnet
add_library(rdcore
    capture/screen_capture.cpp
    codec/h264_encoder.cpp
    codec/h264_decoder.cpp
    input/input_event.cpp
    input/input_injector.cpp
)
target_include_directories(rdcore PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rdcore PUBLIC rdnet)

# FFmpeg（编解码 + 色彩空间转换）
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec libavutil libswscale)
target_link_libraries(rdcore PUBLIC PkgConfig::FFMPEG)

if(WIN32)
    target_sources(rdcore PRIVATE
        capture/capture_win.cpp
        input/input_inject_win.cpp)
    target_link_libraries(rdcore PRIVATE gdi32 user32)
elseif(APPLE)
    target_sources(rdcore PRIVATE
        capture/capture_mac.mm
        input/input_inject_mac.mm)
    target_link_libraries(rdcore PRIVATE
        "-framework CoreGraphics"
        "-framework CoreFoundation"
        "-framework ApplicationServices")
elseif(UNIX)
    find_package(X11 REQUIRED)
    if(NOT X11_XTest_FOUND)
        message(FATAL_ERROR "缺少 XTest 扩展，请安装 libxtst-dev")
    endif()
    target_sources(rdcore PRIVATE
        capture/capture_x11.cpp
        input/input_inject_x11.cpp)
    target_include_directories(rdcore PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(rdcore PRIVATE ${X11_LIBRARIES} ${X11_XTest_LIB})
endif()

# ---- rdp2p: WebRTC P2P 传输（依赖 libdatachannel） ----
if(RD_ENABLE_P2P)
    add_library(rdp2p
        signal/signal_client.cpp
        p2p/rtc_transport.cpp
    )
    target_include_directories(rdp2p PUBLIC ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(rdp2p PUBLIC rdnet LibDataChannel::LibDataChannel)
endif()
