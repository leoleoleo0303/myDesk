cmake_minimum_required(VERSION 3.16)
project(mydesk LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 源码统一为 UTF-8 编码。MSVC 在中文环境(代码页 936)下会把 UTF-8 字节
# 误判为 GBK，导致注释/emoji 解析错乱（C4819 / 常量中有换行符）。
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 可执行文件统一输出到 build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 生成 compile_commands.json，方便 clangd / IDE 索引
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- WebRTC P2P 穿透：libdatachannel ----
option(RD_ENABLE_P2P "启用 WebRTC P2P 穿透 (libdatachannel)" ON)
if(RD_ENABLE_P2P)
    find_package(LibDataChannel CONFIG QUIET)
    if(NOT LibDataChannel_FOUND)
        include(FetchContent)
        set(NO_MEDIA ON CACHE BOOL "" FORCE)
        set(NO_WEBSOCKET ON CACHE BOOL "" FORCE)
        set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
        set(NO_TESTS ON CACHE BOOL "" FORCE)
        FetchContent_Declare(
            libdatachannel
            GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
            GIT_TAG v0.24.5
        )
        FetchContent_MakeAvailable(libdatachannel)
    endif()
endif()

add_subdirectory(src/core)
add_subdirectory(src/apps/capture_demo)
add_subdirectory(src/apps/codec_demo)
add_subdirectory(src/apps/host)
add_subdirectory(src/apps/viewer)
add_subdirectory(src/apps/signal_server)
add_subdirectory(src/apps/gui_viewer)
add_subdirectory(src/apps/mydesk)
if(RD_ENABLE_P2P)
    add_subdirectory(src/apps/rtc_demo)
endif()
