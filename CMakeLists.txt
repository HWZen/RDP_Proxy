project(RDP_PROXY)
cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 20)

option(ASIO_DIR "Path to asio library" "C:/asio-1.24.0")
include_directories(${ASIO_DIR}/include)
add_definitions(-DASIO_SEPARATE_COMPILATION )
#add_definitions(-DASIO_ENABLE_HANDLER_TRACKING)

add_executable(rdp_proxy_server asio.cpp proxy_server.cpp)
add_executable(rdp_server asio.cpp server.cpp)

# if in linux, link pthread
if (UNIX)
    target_compile_options(rdp_proxy_server PUBLIC -static)
    target_link_libraries(rdp_proxy_server pthread)
    target_link_libraries(rdp_server pthread)
endif()