# 设置 C++ 标准为 C++20
set(CMAKE_CXX_STANDARD 20)

# 启用编译命令的导出，生成 compile_commands.json 文件
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 定义用于代码检查的 Sanitizer 标志
set(SANITIZING_FLAGS -fno-sanitize-recover=all -fsanitize=undefined -fsanitize=address)

# 请求编译器提供更多的警告信息
set(CMAKE_BASE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
# 设置 C++ 编译器的警告标志
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wextra -Weffc++ -Werror -Wshadow -Wpointer-arith -Wcast-qual -Wformat=2 -Wno-unqualified-std-cast-call -Wno-non-virtual-dtor")
