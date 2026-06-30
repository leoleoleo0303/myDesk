add_executable(viewer main.cpp)
target_link_libraries(viewer PRIVATE rdcore)

# SDL2 用于实时窗口显示；找不到则只支持无窗口测试模式。
# 优先用 CMake config 包（vcpkg 提供，能按 Debug/Release 正确选库）；
# 找不到再回退到 pkg-config（Linux 系统包常用这种方式）。
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
    endif()
endif()

if(TARGET SDL2::SDL2)
    # CMake config 包的目标
    target_link_libraries(viewer PRIVATE SDL2::SDL2)
    target_compile_definitions(viewer PRIVATE RD_HAVE_SDL2)
    message(STATUS "viewer: 已启用 SDL2 窗口显示 (CMake config)")
elseif(TARGET PkgConfig::SDL2)
    target_link_libraries(viewer PRIVATE PkgConfig::SDL2)
    target_compile_definitions(viewer PRIVATE RD_HAVE_SDL2)
    message(STATUS "viewer: 已启用 SDL2 窗口显示 (pkg-config)")
else()
    message(STATUS "viewer: 未找到 SDL2，仅支持无窗口测试模式")
endif()

if(TARGET rdp2p)
    target_link_libraries(viewer PRIVATE rdp2p)
    target_compile_definitions(viewer PRIVATE RD_HAVE_P2P)
endif()
