# 设置默认的构建类型为 "Debug"
set(default_build_type "Debug")

# 检查当前的构建类型是否与影子构建类型相同
if (NOT (CMAKE_BUILD_TYPE_SHADOW STREQUAL CMAKE_BUILD_TYPE))
    # 如果没有设置 CMAKE_BUILD_TYPE 且没有设置 CMAKE_CONFIGURATION_TYPES
    if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        # 输出状态信息，告知用户将构建类型设置为默认值
        message(STATUS "Setting build type to '${default_build_type}'")
        
        # 将 CMAKE_BUILD_TYPE 设置为默认构建类型，并强制缓存该值
        set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
    endif()
    
    # 将当前的 CMAKE_BUILD_TYPE 存储到 CMAKE_BUILD_TYPE_SHADOW 中，以便后续检测构建类型的变化
    set(CMAKE_BUILD_TYPE_SHADOW ${CMAKE_BUILD_TYPE} CACHE STRING "used to detect changes in build type" FORCE)
endif()

# 输出当前的构建模式
message(STATUS "Building in '${CMAKE_BUILD_TYPE}' mode.")
