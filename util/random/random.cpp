#include <algorithm> // 引入算法库，提供 std::generate
#include <array>     // 引入数组库，提供 std::array
#include <random>    // 引入随机数库，提供 random_device, default_random_engine, seed_seq
#include "random.h"  // 引入自定义的随机数相关头文件

// 函数 get_random_engine 用于生成并返回一个初始化的默认随机数引擎
std::default_random_engine get_random_engine()
{
    // 创建一个随机设备对象，用于生成随机数
    std::random_device rd;

    // 创建一个大小为 1024 的 uint32_t 数组，用于存储种子数据
    std::array<uint32_t, 1024> seed_data{};

    // 使用 std::generate 填充 seed_data 数组
    // 生成 1024 个随机数并存储在 seed_data 中
    std::generate(seed_data.begin(), seed_data.end(), [&] { return rd(); });

    // 使用 seed_data 初始化种子序列
    std::seed_seq seed(seed_data.begin(), seed_data.end());

    // 返回一个使用 seed 初始化的默认随机数引擎
    return std::default_random_engine(seed);
}
