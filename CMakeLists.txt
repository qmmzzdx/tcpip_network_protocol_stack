# 设置 CMake 的最低版本要求
cmake_minimum_required(VERSION 3.24.2)

# 定义项目名称和使用的语言（C++）
project(minnow CXX)

# 如果生成器是 Unix Makefiles，则设置自定义的 make 程序
if(${CMAKE_GENERATOR} STREQUAL "Unix Makefiles")
    set(CMAKE_MAKE_PROGRAM "${PROJECT_SOURCE_DIR}/scripts/make-parallel.sh" CACHE STRING "" FORCE)
endif()

# 检查构建目录是否与源目录相同，如果相同则报错
if(${PROJECT_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "Minnow must be built outside its source directory, e.g. `cmake -B build`.")
endif()

# 包含其他 CMake 脚本文件，通常用于设置构建类型、编译选项等
include(etc/build_type.cmake)  # 包含构建类型设置
include(etc/cflags.cmake)      # 包含编译器标志设置
include(etc/scanners.cmake)    # 包含扫描器相关设置
include(etc/tests.cmake)       # 包含测试相关设置

# 添加 src 目录下的头文件搜索路径
include_directories(
    "${PROJECT_SOURCE_DIR}/src/byte_stream"
    "${PROJECT_SOURCE_DIR}/src/network_interface"
    "${PROJECT_SOURCE_DIR}/src/reassembler"
    "${PROJECT_SOURCE_DIR}/src/router"
    "${PROJECT_SOURCE_DIR}/src/tcp_receiver"
    "${PROJECT_SOURCE_DIR}/src/tcp_sender"
    "${PROJECT_SOURCE_DIR}/src/wrapping_integers"
)

# 添加 tests 目录下的头文件搜索路径
include_directories(
    "${PROJECT_SOURCE_DIR}/tests/byte_stream_test"
    "${PROJECT_SOURCE_DIR}/tests/network_interface_test"
    "${PROJECT_SOURCE_DIR}/tests/reassembler_test"
    "${PROJECT_SOURCE_DIR}/tests/router_test"
    "${PROJECT_SOURCE_DIR}/tests/tcp_receiver_test"
    "${PROJECT_SOURCE_DIR}/tests/tcp_sender_test"
    "${PROJECT_SOURCE_DIR}/tests/tools"
    "${PROJECT_SOURCE_DIR}/tests/wrapping_integers_test"
)

# 添加 util 目录下的头文件搜索路径
include_directories(
    "${PROJECT_SOURCE_DIR}/util/address"
    "${PROJECT_SOURCE_DIR}/util/arp_message"
    "${PROJECT_SOURCE_DIR}/util/ethernet"
    "${PROJECT_SOURCE_DIR}/util/eventloop"
    "${PROJECT_SOURCE_DIR}/util/file_descriptor"
    "${PROJECT_SOURCE_DIR}/util/ipv4_header"
    "${PROJECT_SOURCE_DIR}/util/random"
    "${PROJECT_SOURCE_DIR}/util/socket"
    "${PROJECT_SOURCE_DIR}/util/tcp_minnow_socket"
    "${PROJECT_SOURCE_DIR}/util/tcp_over_ip"
    "${PROJECT_SOURCE_DIR}/util/tcp_segment"
    "${PROJECT_SOURCE_DIR}/util/tools"
    "${PROJECT_SOURCE_DIR}/util/tun"
    "${PROJECT_SOURCE_DIR}/util/tuntap"
)

# 添加子目录，分别处理 util、src、tests 和 apps 目录中的 CMakeLists.txt
add_subdirectory("${PROJECT_SOURCE_DIR}/util")     # 添加 util 子目录
add_subdirectory("${PROJECT_SOURCE_DIR}/src")      # 添加 src 子目录
add_subdirectory("${PROJECT_SOURCE_DIR}/tests")    # 添加 tests 子目录
add_subdirectory("${PROJECT_SOURCE_DIR}/apps")     # 添加 apps 子目录

# 设置 CMake 导出编译命令的选项，生成 compile_commands.json 文件
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
