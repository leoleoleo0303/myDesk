# myDesk 统一客户端（类似 ToDesk 的双向控制桌面应用）
# 同一个程序既可以做被控端（等待别人连接控制本机）又可以做控制端（连接别的机器）。
# 支持 GUI 模式和 headless（无界面纯服务）模式。
#
# GUI 模式: mydesk              (默认，Windows/Linux/Mac 桌面)
# Headless: mydesk --headless   (服务器部署，不依赖显示器)

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
)

# Windows 下使用 WIN32 标志 -> 入口为 WinMain，不会弹出控制台窗口
# 同时在 main.cpp 里我们处理了 console 的隐藏逻辑作为兜底
if(WIN32)
    add_executable(mydesk WIN32 ${MYDESK_SOURCES})
    # WIN32 subsystem 使用 WinMain，但我们仍然用 main()：
    # MSVC linker 可以通过 /ENTRY:mainCRTStartup 让 WIN32 程序使用 main()
    target_link_options(mydesk PRIVATE /ENTRY:mainCRTStartup)
else()
    add_executable(mydesk ${MYDESK_SOURCES})
endif()

target_link_libraries(mydesk PRIVATE rdcore Qt5::Widgets Qt5::Gui Qt5::Core Qt5::Network)

find_package(Threads REQUIRED)
target_link_libraries(mydesk PRIVATE Threads::Threads)

if(TARGET rdp2p)
    target_link_libraries(mydesk PRIVATE rdp2p)
    target_compile_definitions(mydesk PRIVATE RD_HAVE_P2P)
endif()

message(STATUS "mydesk: 已启用 (Qt5 统一客户端, 支持 --headless 服务模式)")
