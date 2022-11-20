project(RDP_PROXY)
cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 20)

option(ASIO_DIR "Path to asio library" "C:/asio-1.24.0")
message(STATUS "ASIO_DIR: ${ASIO_DIR}")
include_directories(${ASIO_DIR}/include)
add_definitions(-DASIO_SEPARATE_COMPILATION )
#add_definitions(-DASIO_ENABLE_HANDLER_TRACKING)

add_executable(rdp_cloud_server asio.cpp cloud_server.cpp)
add_executable(rdp_client_server asio.cpp client_server.cpp)

# if in linux, link pthread
if (UNIX)
    target_link_libraries(rdp_cloud_server pthread)
    target_link_libraries(rdp_client_server pthread)
endif()