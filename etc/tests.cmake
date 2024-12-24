# 包含 CTest 模块，允许使用 CTest 相关的命令和功能
include(CTest)

# 将 CTest 的参数添加到 CMAKE_CTEST_ARGUMENTS 列表中
list(APPEND CMAKE_CTEST_ARGUMENTS --output-on-failure --stop-on-failure --timeout 12 -E 'speed_test|optimization|webget')

# 设置编译测试的名称
set(compile_name "compile with bug-checkers")

# 添加一个测试，名称为 compile_name，执行构建命令以运行功能测试
add_test(NAME ${compile_name}
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" -t functionality_testing webget)

# 定义一个宏 ttest，用于简化添加测试的过程
macro(ttest name)
    # 添加一个测试，名称为 name，命令为 "${name}_sanitized"
    add_test(NAME ${name} COMMAND "${name}_sanitized")
    # 设置测试的属性，要求在运行该测试之前执行 compile 这个 fixture
    set_property(TEST ${name} PROPERTY FIXTURES_REQUIRED compile)
endmacro(ttest)

# 设置 compile_name 测试的超时时间为 0，表示不超时
set_property(TEST ${compile_name} PROPERTY TIMEOUT 0)

# 设置 compile_name 测试的属性，要求在运行该测试之前执行 compile 这个 fixture
set_tests_properties(${compile_name} PROPERTIES FIXTURES_SETUP compile)

# 添加一个测试 t_webget，执行指定的 shell 脚本
add_test(NAME t_webget COMMAND "${PROJECT_SOURCE_DIR}/scripts/webget_t.sh" "${PROJECT_BINARY_DIR}")
# 设置 t_webget 测试的属性，要求在运行该测试之前执行 compile 这个 fixture
set_property(TEST t_webget PROPERTY FIXTURES_REQUIRED compile)

# 使用宏 ttest 添加多个测试
ttest(byte_stream_basics)
ttest(byte_stream_capacity)
ttest(byte_stream_one_write)
ttest(byte_stream_two_writes)
ttest(byte_stream_many_writes)
ttest(byte_stream_stress_test)

ttest(reassembler_single)
ttest(reassembler_cap)
ttest(reassembler_seq)
ttest(reassembler_dup)
ttest(reassembler_holes)
ttest(reassembler_overlapping)
ttest(reassembler_win)

ttest(wrapping_integers_cmp)
ttest(wrapping_integers_wrap)
ttest(wrapping_integers_unwrap)
ttest(wrapping_integers_roundtrip)
ttest(wrapping_integers_extra)

ttest(recv_connect)
ttest(recv_transmit)
ttest(recv_window)
ttest(recv_reorder)
ttest(recv_reorder_more)
ttest(recv_close)
ttest(recv_special)

ttest(send_connect)
ttest(send_transmit)
ttest(send_retx)
ttest(send_window)
ttest(send_ack)
ttest(send_close)
ttest(send_extra)

ttest(net_interface)
ttest(router)

# 创建一个自定义目标 check0，执行 CTest 命令，运行特定的测试
add_custom_target(check0 
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --stop-on-failure --timeout 12 -R 'webget|^byte_stream_'
)

# 创建一个自定义目标 check_webget，执行 CTest 命令，运行 webget 相关的测试
add_custom_target(check_webget 
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 12 -R 'webget'
)

# 创建一个自定义目标 check1，执行 CTest 命令，运行 byte_stream 和 reassembler 相关的测试
add_custom_target(check1 
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --stop-on-failure --timeout 12 -R '^byte_stream_|^reassembler_'
)

# 创建一个自定义目标 check2，执行 CTest 命令，运行 byte_stream、reassembler 和 wrapping 相关的测试
add_custom_target(check2 
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --stop-on-failure --timeout 12 -R '^byte_stream_|^reassembler_|^wrapping|^recv'
)

# 创建一个自定义目标 check3，执行 CTest 命令，运行 byte_stream、reassembler、wrapping、recv 和 send 相关的测试
add_custom_target(check3 
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --stop-on-failure --timeout 12 -R '^byte_stream_|^reassembler_|^wrapping|^recv|^send'
)

# 创建一个自定义目标 check5，执行 CTest 命令，运行 net_interface 相关的测试
add_custom_target(check5 
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --stop-on-failure --timeout 12 -R '^net_interface'
)

# 创建一个自定义目标 check6，执行 CTest 命令，运行 net_interface 和 router 相关的测试
add_custom_target(check6 
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --stop-on-failure --timeout 12 -R '^net_interface|^router'
)

###

# 创建一个自定义目标 speed，执行 CTest 命令，运行 speed_test 相关的测试
add_custom_target(speed 
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --timeout 12 -R '_speed_test'
)

# 设置编译优化测试的名称
set(compile_name_opt "compile with optimization")

# 添加一个测试，名称为 compile_name_opt，执行构建命令以运行速度测试
add_test(NAME ${compile_name_opt}
    COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" -t speed_testing
)

# 定义一个宏 stest，用于简化添加速度测试的过程
macro(stest name)
    # 添加一个测试，名称为 name，命令为 name
    add_test(NAME ${name} COMMAND ${name})
    # 设置测试的属性，要求在运行该测试之前执行 compile_opt 这个 fixture
    set_property(TEST ${name} PROPERTY FIXTURES_REQUIRED compile_opt)
endmacro(stest)

# 设置 compile_name_opt 测试的超时时间为 0，表示不超时
set_property(TEST ${compile_name_opt} PROPERTY TIMEOUT 0)

# 设置 compile_name_opt 测试的属性，要求在运行该测试之前执行 compile_opt 这个 fixture
set_tests_properties(${compile_name_opt} PROPERTIES FIXTURES_SETUP compile_opt)

# 使用宏 stest 添加多个速度测试
stest(byte_stream_speed_test)
stest(reassembler_speed_test)
