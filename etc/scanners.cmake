# 使用 GLOB_RECURSE 查找 src 目录及其子目录下的所有 .cpp 文件
file(GLOB_RECURSE MINNOW_CPP_FILES ${CMAKE_SOURCE_DIR}/src/*.cpp)

# 使用 GLOB_RECURSE 查找当前目录及其子目录下的所有 .h 和 .cpp 文件
file(GLOB_RECURSE ALL_SRC_FILES *.h *.cpp)

# 创建一个自定义目标 'format'，用于格式化源代码
# 使用 clang-format 工具对 ALL_SRC_FILES 中的文件进行格式化
add_custom_target(format 
    COMMAND "clang-format" -i ${ALL_SRC_FILES} 
    COMMENT "Formatting source code..."
)

# 遍历 ALL_SRC_FILES 中的每一个文件
foreach(tidy_target ${ALL_SRC_FILES})
    # 获取文件名（不带路径）
    get_filename_component(basename ${tidy_target} NAME)
    
    # 获取文件所在目录的路径
    get_filename_component(dirname ${tidy_target} DIRECTORY)
    
    # 获取目录的名称（不带路径）
    get_filename_component(basedir ${dirname} NAME)
    
    # 创建一个新的目标名称，格式为 '目录名__文件名'
    set(tidy_target_name "${basedir}__${basename}")
    
    # 设置 clang-tidy 命令，-quiet 表示安静模式，-header-filter 用于过滤头文件，-p 指定构建目录
    set(tidy_command clang-tidy --quiet -header-filter=.* -p=${PROJECT_BINARY_DIR} ${tidy_target})
    
    # 创建一个自定义目标，名称为 'tidy_目录名__文件名'，执行 clang-tidy 命令
    add_custom_target(tidy_${tidy_target_name} 
        COMMAND ${tidy_command}
    )
    
    # 将新创建的目标添加到 ALL_TIDY_TARGETS 列表中
    list(APPEND ALL_TIDY_TARGETS tidy_${tidy_target_name})

    # 如果当前文件在 MINNOW_CPP_FILES 列表中，则将其对应的 tidy 目标添加到 MINNOW_TIDY_TARGETS 列表中
    if(${tidy_target} IN_LIST MINNOW_CPP_FILES)
        list(APPEND MINNOW_TIDY_TARGETS tidy_${tidy_target_name})
    endif()
endforeach(tidy_target)

# 创建一个自定义目标 'tidy'，依赖于 MINNOW_TIDY_TARGETS 中的所有目标
add_custom_target(tidy 
    DEPENDS ${MINNOW_TIDY_TARGETS}
)

# 创建一个自定义目标 'tidy-all'，依赖于 ALL_TIDY_TARGETS 中的所有目标
add_custom_target(tidy-all 
    DEPENDS ${ALL_TIDY_TARGETS}
)
