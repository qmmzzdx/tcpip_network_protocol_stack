#include <iostream>                   // 引入输入输出流库，用于输出信息
#include <algorithm>                  // 引入算法库，提供常用算法，如 shuffle 和 generate
#include <cstdint>                    // 引入 C 标准库，提供固定宽度整数类型
#include <exception>                  // 引入异常处理库
#include <tuple>                      // 引入元组库，提供 std::tuple 类型
#include <vector>                     // 引入向量库，提供 std::vector 类型
#include "random.h"                   // 引入随机数生成相关的自定义头文件
#include "reassembler_test_harness.h" // 引入 Reassembler 测试工具的定义
using namespace std;

static constexpr size_t NREPS = 32;         // 重复测试的次数
static constexpr size_t NSEGS = 128;        // 每次测试的段数
static constexpr size_t MAX_SEG_LEN = 2048; // 每个段的最大长度

int main()
{
    try
    {
        auto rd = get_random_engine(); // 获取随机数生成器

        // 重叠段测试
        for (unsigned rep_no = 0; rep_no < NREPS; ++rep_no)
        {
            // 创建 Reassembler 测试工具实例，指定测试名称和容量
            ReassemblerTestHarness sr{"win test " + to_string(rep_no), NSEGS * MAX_SEG_LEN};

            vector<tuple<size_t, size_t>> seq_size; // 存储段的偏移和大小
            size_t offset = 0;                      // 初始化偏移量
            for (unsigned i = 0; i < NSEGS; ++i)
            {
                // 随机生成段的大小和偏移
                const size_t size = 1 + (rd() % (MAX_SEG_LEN - 1));                      // 段大小在 1 到 MAX_SEG_LEN 之间
                const size_t offs = min(offset, 1 + (static_cast<size_t>(rd()) % 1023)); // 随机偏移
                seq_size.emplace_back(offset - offs, size + offs);                       // 将偏移和大小存入元组
                offset += size;                                                          // 更新总偏移量
            }
            shuffle(seq_size.begin(), seq_size.end(), rd); // 随机打乱段的顺序

            string d(offset, 0);             // 创建一个字符串，大小为总偏移量
            generate(d.begin(), d.end(), [&] { return rd(); });

            // 执行插入操作
            for (auto [off, sz] : seq_size)
            {
                sr.execute(Insert{d.substr(off, sz), off}.is_last(off + sz == offset)); // 插入段并标记是否为最后一个段
            }
            sr.execute(ReadAll{d}); // 读取所有数据
        }
    }
    catch (const exception &e) // 捕获异常
    {
        cerr << "Exception: " << e.what() << endl; // 输出异常信息
        return EXIT_FAILURE;                       // 返回失败状态
    }
    return EXIT_SUCCESS; // 返回成功状态
}
