#include <iostream>      // 引入输入输出流库，用于输出信息
#include <chrono>        // 引入时间库，用于测量运行时间
#include <cstddef>       // 引入 C 标准库，提供 size_t 类型
#include <fstream>       // 引入文件流库，用于文件操作
#include <iomanip>       // 引入格式化库，用于设置输出格式
#include <algorithm>     // 引入算法库，提供常用算法
#include <queue>         // 引入队列库，提供队列数据结构
#include <random>        // 引入随机数库，提供随机数生成
#include <tuple>         // 引入元组库，提供 std::tuple 类型
#include "reassembler.h" // 引入 Reassembler 类的定义

using namespace std;         // 使用标准命名空间，简化代码书写
using namespace std::chrono; // 使用 chrono 命名空间，简化时间相关代码

// speed_test 函数用于测试 Reassembler 的性能
void speed_test(const size_t num_chunks,  // NOLINT(bugprone-easily-swappable-parameters)
                const size_t capacity,    // NOLINT(bugprone-easily-swappable-parameters)
                const size_t random_seed) // NOLINT(bugprone-easily-swappable-parameters)
{
    // 生成要写入的数据
    const string data = [&]
    {
        default_random_engine rd{random_seed}; // 创建随机数生成器，使用给定的种子
        uniform_int_distribution<char> ud;     // 创建均匀分布的字符生成器
        string ret;                            // 用于存储生成的数据
        for (size_t i = 0; i < num_chunks * capacity; ++i)
        {
            ret += ud(rd); // 生成随机字符并添加到数据中
        }
        return ret; // 返回生成的数据
    }();

    // 将数据分割成多个段
    queue<tuple<uint64_t, string, bool>> split_data; // 创建一个队列，用于存储分割后的数据
    for (size_t i = 0; i < data.size(); i += capacity)
    {
        // 将数据分割成多个片段，使用元组存储每个片段的起始位置、内容和是否为最后一个片段
        split_data.emplace(i + 2, data.substr(i + 2, capacity * 2), i + 2 + capacity * 2 >= data.size());
        split_data.emplace(i, data.substr(i, capacity * 2), i + capacity * 2 >= data.size());
        split_data.emplace(i + 1, data.substr(i + 1, capacity * 2), i + 1 + capacity * 2 >= data.size());
    }

    // 创建 Reassembler 实例，初始化 ByteStream
    Reassembler reassembler{ByteStream{capacity}};

    string output_data;               // 用于存储重组后的数据
    output_data.reserve(data.size()); // 预留空间以提高性能

    const auto start_time = steady_clock::now(); // 记录开始时间
    while (not split_data.empty())               // 当队列不为空时
    {
        auto &next = split_data.front();                                                   // 获取队列的前一个元素
        reassembler.insert(get<uint64_t>(next), move(get<string>(next)), get<bool>(next)); // 将数据插入 Reassembler
        split_data.pop();                                                                  // 移除已处理的元素

        // 从 Reassembler 中读取数据
        while (reassembler.reader().bytes_buffered()) // 当有缓冲数据时
        {
            output_data += reassembler.reader().peek();                                         // 读取数据并添加到 output_data
            reassembler.reader().pop(output_data.size() - reassembler.reader().bytes_popped()); // 从缓冲区中移除已读取的数据
        }
    }

    const auto stop_time = steady_clock::now(); // 记录结束时间

    // 检查 Reassembler 是否正确关闭 ByteStream
    if (not reassembler.reader().is_finished())
    {
        throw runtime_error("Reassembler did not close ByteStream when finished");
    }

    // 检查重组后的数据是否与原始数据匹配
    if (data != output_data)
    {
        throw runtime_error("Mismatch between data written and read");
    }

    // 计算测试持续时间
    auto test_duration = duration_cast<duration<double>>(stop_time - start_time);
    auto bytes_per_second = static_cast<double>(num_chunks * capacity) / test_duration.count(); // 计算每秒字节数
    auto bits_per_second = 8 * bytes_per_second;                                                // 计算每秒比特数
    auto gigabits_per_second = bits_per_second / 1e9;                                           // 转换为吉比特每秒

    fstream debug_output;          // 创建文件流对象
    debug_output.open("/dev/tty"); // 打开终端设备

    // 输出测试结果
    cout << "Reassembler to ByteStream with capacity=" << capacity << " reached " << fixed << setprecision(2)
         << gigabits_per_second << " Gbit/s.\n";

    debug_output << "             Reassembler throughput: " << fixed << setprecision(2) << gigabits_per_second
                 << " Gbit/s\n";

    // 检查是否达到最低速度要求
    if (gigabits_per_second < 0.1)
    {
        throw runtime_error("Reassembler did not meet minimum speed of 0.1 Gbit/s.");
    }
}

// 执行速度测试
void program_body()
{
    speed_test(10000, 1500, 1370); // 调用 speed_test，生成 10000 个数据块，每个块容量为 1500 字节，随机种子为 1370
}

int main()
{
    try
    {
        program_body();
    }
    catch (const exception &e)
    {
        cerr << "Exception: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
