#include <iostream>                   // 引入输入输出流库，用于标准输入输出
#include <random>                     // 引入随机数库，用于生成随机数据
#include "byte_stream_test_harness.h" // 引入自定义的 ByteStream 测试工具头文件
using namespace std;

// stress_test 函数用于对 ByteStream 进行压力测试
void stress_test(const size_t input_len,   // 输入数据的长度
                 const size_t capacity,    // ByteStream 的容量
                 const size_t random_seed) // 随机数种子
{
    default_random_engine rd{random_seed}; // 创建随机数生成器，使用给定的种子

    // 生成要写入的数据
    const string data = [&rd, &input_len]
    {
        uniform_int_distribution<char> ud; // 创建均匀分布的字符生成器
        string ret;                        // 用于存储生成的数据
        for (size_t i = 0; i < input_len; ++i)
        {                  // 生成 input_len 个随机字符
            ret += ud(rd); // 将生成的字符添加到字符串中
        }
        return ret; // 返回生成的数据
    }();

    // 创建 ByteStream 测试工具对象
    ByteStreamTestHarness bs{"stress test input=" + to_string(input_len) + ", capacity=" + to_string(capacity), capacity};

    // 初始化期望的字节计数
    size_t expected_bytes_pushed{};               // 期望推送的字节数
    size_t expected_bytes_popped{};               // 期望弹出的字节数
    size_t expected_available_capacity{capacity}; // 期望的可用容量

    // 循环直到所有数据都被推送和弹出
    while (expected_bytes_pushed < data.size() || expected_bytes_popped < data.size())
    {
        // 执行状态检查
        bs.execute(BytesPushed{expected_bytes_pushed});
        bs.execute(BytesPopped{expected_bytes_popped});
        bs.execute(AvailableCapacity{expected_available_capacity});
        bs.execute(BytesBuffered{expected_bytes_pushed - expected_bytes_popped});

        /* 写入数据 */
        uniform_int_distribution<size_t> bytes_to_push_dist{0, data.size() - expected_bytes_pushed}; // 创建推送字节数的分布
        const size_t amount_to_push = bytes_to_push_dist(rd);                                        // 随机决定要推送的字节数
        bs.execute(Push{data.substr(expected_bytes_pushed, amount_to_push)});                        // 将数据推送到 ByteStream
        expected_bytes_pushed += min(amount_to_push, expected_available_capacity);                   // 更新推送的字节数
        expected_available_capacity -= min(amount_to_push, expected_available_capacity);             // 更新可用容量

        // 执行状态检查
        bs.execute(BytesPushed{expected_bytes_pushed});
        bs.execute(AvailableCapacity{expected_available_capacity});

        // 如果所有数据都已推送，关闭写入器
        if (expected_bytes_pushed == data.size())
        {
            bs.execute(Close{});
        }

        /* 读取数据 */
        const size_t peek_size = bs.peek_size(); // 获取可读取的字节数
        if ((expected_bytes_pushed != expected_bytes_popped) && (peek_size == 0))
        {
            throw runtime_error("ByteStream::reader().peek() returned empty view"); // 抛出异常
        }
        if (expected_bytes_popped + peek_size > expected_bytes_pushed)
        {
            throw runtime_error("ByteStream::reader().peek() returned too-large view"); // 抛出异常
        }

        bs.execute(PeekOnce{data.substr(expected_bytes_popped, peek_size)}); // 预览要读取的数据

        uniform_int_distribution<size_t> bytes_to_pop_dist{0, peek_size}; // 创建弹出字节数的分布
        const size_t amount_to_pop = bytes_to_pop_dist(rd);               // 随机决定要弹出的字节数

        bs.execute(Pop{amount_to_pop});                 // 从 ByteStream 中弹出数据
        expected_bytes_popped += amount_to_pop;         // 更新弹出的字节数
        expected_available_capacity += amount_to_pop;   // 更新可用容量
        bs.execute(BytesPopped{expected_bytes_popped}); // 执行状态检查
    }

    // 执行最终状态检查
    bs.execute(IsClosed{true});
    bs.execute(IsFinished{true});
}

// program_body 函数用于执行多个压力测试
void program_body()
{
    stress_test(19, 3, 10110);      // 执行压力测试，输入长度为 19，容量为 3，随机种子为 10110
    stress_test(18, 17, 12345);     // 执行压力测试，输入长度为 18，容量为 17，随机种子为 12345
    stress_test(1111, 17, 98765);   // 执行压力测试，输入长度为 1111，容量为 17，随机种子为 98765
    stress_test(4097, 4096, 11101); // 执行压力测试，输入长度为 4097，容量为 4096，随机种子为 11101
}

// main 函数是程序的入口
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
