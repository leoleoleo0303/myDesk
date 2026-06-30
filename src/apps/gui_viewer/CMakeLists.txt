# Qt 桌面控制端。找不到 Qt5 则跳过该目标。
find_package(Qt5 COMPONENTS Widgets Gui Core QUIET)
if(NOT Qt5_FOUND)
    message(STATUS "gui_viewer: 未找到 Qt5，跳过（安装 qtbase5-dev 后启用）")
    return()
endif()

set(CMAKE_AUTOMOC ON)

add_executable(gui_viewer
    main.cpp
    main_window.cpp
    video_widget.cpp
    remote_session.cpp
)
target_link_libraries(gui_viewer PRIVATE rdcore Qt5::Widgets Qt5::Gui Qt5::Core)

find_package(Threads REQUIRED)
target_link_libraries(gui_viewer PRIVATE Threads::Threads)

if(TARGET rdp2p)
    target_link_libraries(gui_viewer PRIVATE rdp2p)
    target_compile_definitions(gui_viewer PRIVATE RD_HAVE_P2P)
endif()

message(STATUS "gui_viewer: 已启用 (Qt5)")
