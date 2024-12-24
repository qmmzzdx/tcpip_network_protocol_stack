# 创建一个名为 stream_copy 的静态库，包含 bidirectional_stream_copy.cpp 源文件
add_library(stream_copy STATIC bidirectional_stream_copy.cpp)

# 创建一个名为 stream_sanitized 的静态库，排除默认构建，包含 bidirectional_stream_copy.cpp 源文件
add_library(stream_sanitized EXCLUDE_FROM_ALL STATIC bidirectional_stream_copy.cpp)

# 为 stream_sanitized 库添加编译选项，使用 SANITIZING_FLAGS 变量中的选项
target_compile_options(stream_sanitized PUBLIC ${SANITIZING_FLAGS})

# 定义一个宏，用于添加可执行文件
macro(add_app exec_name)
    # 创建一个可执行文件，名称为 exec_name，源文件为 exec_name.cpp
    add_executable("${exec_name}" "${exec_name}.cpp")

    # 将 stream_copy 库链接到可执行文件
    target_link_libraries("${exec_name}" stream_copy)

    # 将 minnow_debug 库链接到可执行文件
    target_link_libraries("${exec_name}" minnow_debug)

    # 将 util_debug 库链接到可执行文件
    target_link_libraries("${exec_name}" util_debug)

    # 创建一个名为 exec_name_sanitized 的可执行文件，源文件为 exec_name.cpp
    add_executable("${exec_name}_sanitized" "${exec_name}.cpp")

    # 为 exec_name_sanitized 可执行文件添加编译选项，使用 SANITIZING_FLAGS 变量中的选项
    target_compile_options("${exec_name}_sanitized" PUBLIC ${SANITIZING_FLAGS})

    # 为 exec_name_sanitized 可执行文件添加链接选项，使用 SANITIZING_FLAGS 变量中的选项
    target_link_options("${exec_name}_sanitized" PUBLIC ${SANITIZING_FLAGS})

    # 将 stream_sanitized 库链接到 exec_name_sanitized 可执行文件
    target_link_libraries("${exec_name}_sanitized" stream_sanitized)

    # 将 minnow_sanitized 库链接到 exec_name_sanitized 可执行文件
    target_link_libraries("${exec_name}_sanitized" minnow_sanitized)

    # 将 util_sanitized 库链接到 exec_name_sanitized 可执行文件
    target_link_libraries("${exec_name}_sanitized" util_sanitized)
endmacro(add_app)

# 调用 add_app 宏，创建多个可执行文件
add_app(webget)        # 创建可执行文件 webget 和 webget_sanitized
add_app(tcp_native)    # 创建可执行文件 tcp_native 和 tcp_native_sanitized
add_app(tcp_ipv4)      # 创建可执行文件 tcp_ipv4 和 tcp_ipv4_sanitized
add_app(endtoend)      # 创建可执行文件 endtoend 和 endtoend_sanitized
