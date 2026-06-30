add_executable(host main.cpp)
target_link_libraries(host PRIVATE rdcore)

find_package(Threads REQUIRED)
target_link_libraries(host PRIVATE Threads::Threads)

if(TARGET rdp2p)
    target_link_libraries(host PRIVATE rdp2p)
    target_compile_definitions(host PRIVATE RD_HAVE_P2P)
endif()
