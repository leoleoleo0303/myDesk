# myDesk 统一客户端（类似 ToDesk 的双向控制桌面应用）
# 同一个程序既可以做被控端（等待别人连接控制本机）又可以做控制端（连接别的机器）。
# 支持 GUI 模式和 headless（无界面纯服务）模式。
#
# 新增功能:
#   - 账号登录 (开发者/用户模式)
#   - 开发者日志面板 (一键复制)
#   - 文件传输 (拖拽支持、双向传输)
#   - 语音通话
#   - 设备列表 (一键连接)
#   - Web 远程桌面服务 (HTML + WebSocket)

find_package(Qt5 COMPONENTS Widgets Gui Core Network QUIET)
if(NOT Qt5_FOUND)
    message(STATUS "mydesk: 未找到 Qt5，跳过（安装 qtbase5-dev / Qt5 后启用）")
    return()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(MYDESK_SOURCES
    main.cpp
    frameless_window.cpp
    mydesk_window.cpp
    host_service.cpp
    viewer_session.cpp
    viewer_widget.cpp
    device_id_gen.cpp
    headless_service.cpp
    # 新增
    account_manager.cpp
    device_list_manager.cpp
    log_panel.cpp
    login_dialog.cpp
    file_transfer_widget.cpp
    web_server.cpp
)

# Windows 下使用 WIN32 标志 -> 入口为 WinMain，不会弹出控制台窗口
if(WIN32)
    add_executable(mydesk WIN32 ${MYDESK_SOURCES})
    target_link_options(mydesk PRIVATE /ENTRY:mainCRTStartup)
else()
    add_executable(mydesk ${MYDESK_SOURCES})
endif()

target_link_libraries(mydesk PRIVATE
    rdcore
    rdfiletransfer
    rdaudio
    Qt5::Widgets
    Qt5::Gui
    Qt5::Core
    Qt5::Network
)

find_package(Threads REQUIRED)
target_link_libraries(mydesk PRIVATE Threads::Threads)

if(TARGET rdp2p)
    target_link_libraries(mydesk PRIVATE rdp2p)
    target_compile_definitions(mydesk PRIVATE RD_HAVE_P2P)
endif()

message(STATUS "mydesk: 已启用 (Qt5 统一客户端, 含 Web 服务/文件传输/语音/设备列表)")
