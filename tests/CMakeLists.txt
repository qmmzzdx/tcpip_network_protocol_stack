# 创建一个名为 minnow_testing_debug 的静态库，包含 common.cpp 文件
add_library(minnow_testing_debug STATIC tools/common.cpp)

# 创建一个名为 minnow_testing_sanitized 的静态库，包含 common.cpp 文件
# EXCLUDE_FROM_ALL 表示该库不会被默认构建，只有在显式请求时才会构建
add_library(minnow_testing_sanitized EXCLUDE_FROM_ALL STATIC tools/common.cpp)

# 为 minnow_testing_sanitized 库添加编译选项，使用 SANITIZING_FLAGS 变量中的选项
# 这些选项通常用于启用内存检查或其他安全检查
target_compile_options(minnow_testing_sanitized PUBLIC ${SANITIZING_FLAGS})

# 创建一个自定义目标 functionality_testing，用于组织功能测试
# 该目标没有实际的构建规则
add_custom_target(functionality_testing)

# 创建另一个自定义目标 speed_testing，用于组织速度测试
add_custom_target(speed_testing)

# 定义一个宏 add_test_exec，用于简化添加测试可执行文件的过程
# cur_path 表示文件的路径
# exec_name 是传入的参数，表示测试的名称
macro(add_test_exec cur_path exec_name)
    # 创建一个名为 ${exec_name}_sanitized 的可执行文件，包含 ${exec_name}.cpp 源文件
    # EXCLUDE_FROM_ALL 表示该可执行文件不会被默认构建
    add_executable("${exec_name}_sanitized" EXCLUDE_FROM_ALL "${cur_path}/${exec_name}.cpp")

    # 为 ${exec_name}_sanitized 可执行文件添加编译选项，使用 SANITIZING_FLAGS 变量
    target_compile_options("${exec_name}_sanitized" PUBLIC ${SANITIZING_FLAGS})

    # 为 ${exec_name}_sanitized 可执行文件添加链接选项，使用 SANITIZING_FLAGS 变量
    target_link_options("${exec_name}_sanitized" PUBLIC ${SANITIZING_FLAGS})

    # 将 minnow_testing_sanitized 库链接到 ${exec_name}_sanitized 可执行文件
    target_link_libraries("${exec_name}_sanitized" minnow_testing_sanitized)

    # 将 minnow_sanitized 库链接到 ${exec_name}_sanitized 可执行文件
    target_link_libraries("${exec_name}_sanitized" minnow_sanitized)

    # 将 util_sanitized 库链接到 ${exec_name}_sanitized 可执行文件
    target_link_libraries("${exec_name}_sanitized" util_sanitized)

    # 将 ${exec_name}_sanitized 可执行文件添加为 functionality_testing 目标的依赖
    add_dependencies(functionality_testing "${exec_name}_sanitized")

    # 创建一个名为 ${exec_name} 的可执行文件，包含 ${exec_name}.cpp 源文件
    add_executable("${exec_name}" EXCLUDE_FROM_ALL "${cur_path}/${exec_name}.cpp")

    # 将 minnow_testing_debug 库链接到 ${exec_name} 可执行文件
    target_link_libraries("${exec_name}" minnow_testing_debug)

    # 将 minnow_debug 库链接到 ${exec_name} 可执行文件
    target_link_libraries("${exec_name}" minnow_debug)

    # 将 util_debug 库链接到 ${exec_name} 可执行文件
    target_link_libraries("${exec_name}" util_debug)

    # 将 ${exec_name} 可执行文件添加为 functionality_testing 目标的依赖
    add_dependencies(functionality_testing "${exec_name}")
endmacro(add_test_exec)

# 定义一个宏 add_speed_test，用于简化添加速度测试可执行文件的过程
macro(add_speed_test cur_path exec_name)
    # 创建一个名为 ${exec_name} 的可执行文件，包含 ${exec_name}.cpp 源文件
    add_executable("${exec_name}" EXCLUDE_FROM_ALL "${cur_path}/${exec_name}.cpp")

    # 为 ${exec_name} 可执行文件添加优化编译选项
    target_compile_options("${exec_name}" PUBLIC "-O2")

    # 将 minnow_optimized 库链接到 ${exec_name} 可执行文件
    target_link_libraries("${exec_name}" minnow_optimized)

    # 将 util_optimized 库链接到 ${exec_name} 可执行文件
    target_link_libraries("${exec_name}" util_optimized)

    # 将 ${exec_name} 可执行文件添加为 speed_testing 目标的依赖
    add_dependencies(speed_testing "${exec_name}")
endmacro(add_speed_test)

# 添加多个测试可执行文件，使用 add_test_exec 宏
add_test_exec(byte_stream_test byte_stream_basics)
add_test_exec(byte_stream_test byte_stream_capacity)
add_test_exec(byte_stream_test byte_stream_one_write)
add_test_exec(byte_stream_test byte_stream_two_writes)
add_test_exec(byte_stream_test byte_stream_many_writes)
add_test_exec(byte_stream_test byte_stream_stress_test)

add_test_exec(reassembler_test reassembler_single)
add_test_exec(reassembler_test reassembler_cap)
add_test_exec(reassembler_test reassembler_seq)
add_test_exec(reassembler_test reassembler_dup)
add_test_exec(reassembler_test reassembler_holes)
add_test_exec(reassembler_test reassembler_overlapping)
add_test_exec(reassembler_test reassembler_win)

add_test_exec(wrapping_integers_test wrapping_integers_cmp)
add_test_exec(wrapping_integers_test wrapping_integers_wrap)
add_test_exec(wrapping_integers_test wrapping_integers_unwrap)
add_test_exec(wrapping_integers_test wrapping_integers_roundtrip)
add_test_exec(wrapping_integers_test wrapping_integers_extra)

add_test_exec(tcp_receiver_test recv_connect)
add_test_exec(tcp_receiver_test recv_transmit)
add_test_exec(tcp_receiver_test recv_window)
add_test_exec(tcp_receiver_test recv_reorder)
add_test_exec(tcp_receiver_test recv_reorder_more)
add_test_exec(tcp_receiver_test recv_close)
add_test_exec(tcp_receiver_test recv_special)

add_test_exec(tcp_sender_test send_connect)
add_test_exec(tcp_sender_test send_transmit)
add_test_exec(tcp_sender_test send_retx)
add_test_exec(tcp_sender_test send_window)
add_test_exec(tcp_sender_test send_ack)
add_test_exec(tcp_sender_test send_close)
add_test_exec(tcp_sender_test send_extra)

add_test_exec(network_interface_test net_interface)

add_test_exec(router_test router)

# 添加速度测试可执行文件，使用 add_speed_test 宏
add_speed_test(byte_stream_test byte_stream_speed_test)
add_speed_test(reassembler_test reassembler_speed_test)
