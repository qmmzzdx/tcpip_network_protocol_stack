# 递归查找当前目录及其子目录下的所有 .cpp 文件
file(GLOB_RECURSE LIB_SOURCES "*.cpp")

# 创建 minnow_debug 静态库
add_library(minnow_debug STATIC ${LIB_SOURCES})

# 创建 minnow_sanitized 静态库，排除默认构建
add_library(minnow_sanitized EXCLUDE_FROM_ALL STATIC ${LIB_SOURCES})
target_compile_options(minnow_sanitized PUBLIC ${SANITIZING_FLAGS})

# 创建 minnow_optimized 静态库，排除默认构建
add_library(minnow_optimized EXCLUDE_FROM_ALL STATIC ${LIB_SOURCES})
target_compile_options(minnow_optimized PUBLIC "-O2")
