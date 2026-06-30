# 信令服务器只依赖 rdnet（不需要 FFmpeg/X11），便于部署到最小化服务器
add_executable(signal_server main.cpp)
target_link_libraries(signal_server PRIVATE rdnet)

find_package(Threads REQUIRED)
target_link_libraries(signal_server PRIVATE Threads::Threads)
