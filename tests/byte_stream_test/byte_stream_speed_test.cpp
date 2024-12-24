#include <iostream>      // 引入输入输出流库，用于标准输入输出
#include <queue>         // 引入队列库，用于存储分段数据
#include <random>        // 引入随机数库，用于生成随机数据
#include <chrono>        // 引入时间库，用于测量时间
#include <cstddef>       // 引入cstddef库，提供size_t类型
#include <fstream>       // 引入文件流库，用于文件操作
#include <iomanip>       // 引入iomanip库，用于格式化输出
#include "byte_stream.h" // 引入自定义的ByteStream类头文件

using namespace std;         // 使用标准命名空间
using namespace std::chrono; // 使用chrono命名空间，方便使用时间相关的功能

// speed_test函数用于测试ByteStream的读写性能
void speed_test(const size_t input_len,   // 输入数据的长度
                const size_t capacity,    // ByteStream的容量
                const size_t random_seed, // 随机数种子
                const size_t write_size,  // 每次写入的大小
                const size_t read_size)   // 每次读取的大小
{
    // 生成要写入的数据
    const string data = [&random_seed, &input_len]
    {
        default_random_engine rd{random_seed}; // 创建随机数生成器
        uniform_int_distribution<char> ud;     // 创建均匀分布的字符生成器
        string ret;                            // 用于存储生成的数据
        for (size_t i = 0; i < input_len; ++i)
        {                  // 生成input_len个随机字符
            ret += ud(rd); // 将生成的字符添加到字符串中
        }
        return ret; // 返回生成的数据
    }();

    // 将数据分割成多个段以便写入
    queue<string> split_data; // 创建一个队列用于存储分段数据
    for (size_t i = 0; i < data.size(); i += write_size)
    {                                                   // 按照write_size分割数据
        split_data.emplace(data.substr(i, write_size)); // 将分割的数据段添加到队列中
    }

    ByteStream bs{capacity};          // 创建一个ByteStream对象，指定容量
    string output_data;               // 用于存储读取的数据
    output_data.reserve(data.size()); // 预留空间以提高性能

    const auto start_time = steady_clock::now(); // 记录开始时间
    while (!bs.reader().is_finished())
    { // 当读取器未完成时循环
        if (split_data.empty())
        { // 如果没有更多数据可写
            if (!bs.writer().is_closed())
            {                        // 如果写入器未关闭
                bs.writer().close(); // 关闭写入器
            }
        }
        else
        {
            // 如果队列中的数据段可以写入
            if (split_data.front().size() <= bs.writer().available_capacity())
            {
                bs.writer().push(move(split_data.front())); // 将数据段写入ByteStream
                split_data.pop();                           // 从队列中移除已写入的数据段
            }
        }

        // 如果有数据缓冲在读取器中
        if (bs.reader().bytes_buffered())
        {
            auto peeked = bs.reader().peek().substr(0, read_size); // 预览要读取的数据
            if (peeked.empty())
            {                                                                           // 如果预览的数据为空
                throw runtime_error("ByteStream::reader().peek() returned empty view"); // 抛出异常
            }
            output_data += peeked;          // 将预览的数据添加到输出数据中
            bs.reader().pop(peeked.size()); // 从读取器中移除已读取的数据
        }
    }

    const auto stop_time = steady_clock::now(); // 记录结束时间

    // 检查写入的数据和读取的数据是否一致
    if (data != output_data)
    {
        throw runtime_error("Mismatch between data written and read"); // 抛出异常
    }

    // 计算测试持续时间
    auto test_duration = duration_cast<duration<double>>(stop_time - start_time);
    auto bytes_per_second = static_cast<double>(input_len) / test_duration.count(); // 计算每秒字节数
    auto bits_per_second = 8 * bytes_per_second;                                    // 计算每秒比特数
    auto gigabits_per_second = bits_per_second / 1e9;                               // 转换为吉比特每秒

    fstream debug_output;          // 创建文件流对象
    debug_output.open("/dev/tty"); // 打开终端设备

    // 输出测试结果
    cout << "ByteStream with capacity=" << capacity << ", write_size=" << write_size
         << ", read_size=" << read_size << " reached " << fixed << setprecision(2)
         << gigabits_per_second << " Gbit/s.\n";

    debug_output << "             ByteStream throughput: " << fixed << setprecision(2)
                 << gigabits_per_second << " Gbit/s\n";

    // 检查速度是否达到最低要求
    if (gigabits_per_second < 0.1)
    {
        throw runtime_error("ByteStream did not meet minimum speed of 0.1 Gbit/s."); // 抛出异常
    }
}

// program_body函数用于执行速度测试
void program_body()
{
    speed_test(1e7, 32768, 789, 1500, 128); // 调用speed_test函数，传入参数
}

int main()
{
    try
    {
        program_body(); // 执行程序主体
    }
    catch (const exception &e)
    {                                              // 捕获异常
        cerr << "Exception: " << e.what() << "\n"; // 输出异常信息
        return EXIT_FAILURE;                       // 返回失败状态
    }
    return EXIT_SUCCESS; // 返回成功状态
}
