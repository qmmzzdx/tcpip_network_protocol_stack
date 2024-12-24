#ifndef REASSEMBLER_TEST_HARNESS_H
#define REASSEMBLER_TEST_HARNESS_H

#include <optional>                   // 引入可选类型库，提供 std::optional
#include <sstream>                    // 引入字符串流库，用于字符串操作
#include <utility>                    // 引入实用工具库，提供 std::move 等功能
#include "byte_stream_test_harness.h" // 引入 ByteStream 测试工具的定义
#include "common.h"                   // 引入公共定义和工具
#include "reassembler.h"              // 引入 Reassembler 类的定义

// ReassemblerTestStep 模板结构体，用于将 ByteStream 测试步骤封装为 Reassembler 测试步骤
template <std::derived_from<TestStep<ByteStream>> T>
struct ReassemblerTestStep : public TestStep<Reassembler>
{
    T step_; // 存储 ByteStream 测试步骤的实例

    // 构造函数，接受一个 ByteStream 测试步骤并初始化
    template <typename... Targs>
    explicit ReassemblerTestStep(T byte_stream_test_step)
        : TestStep<Reassembler>(), step_(std::move(byte_stream_test_step)) // 初始化基类和步骤
    {
    }

    // 返回步骤的字符串表示
    std::string str() const override { return step_.str(); }

    // 返回步骤的颜色
    uint8_t color() const override { return step_.color(); }

    // 执行步骤，传入 Reassembler 的引用
    void execute(Reassembler &r) const override { step_.execute(r.reader()); }

    // 执行步骤（常量版本），传入 Reassembler 的常量引用
    void execute(const Reassembler &r) const { step_.execute(r.reader()); }
};

// ReassemblerTestHarness 类，继承自 TestHarness<Reassembler>
class ReassemblerTestHarness : public TestHarness<Reassembler>
{
public:
    // 构造函数，接受测试名称和容量，初始化基类
    ReassemblerTestHarness(std::string test_name, uint64_t capacity)
        : TestHarness(move(test_name),
                      "capacity=" + std::to_string(capacity),
                      {Reassembler{ByteStream{capacity}}}) // 初始化 Reassembler 和 ByteStream
    {
    }

    // 执行测试步骤，接受一个 ByteStream 测试步骤
    template <std::derived_from<TestStep<ByteStream>> T>
    void execute(const T &test)
    {
        TestHarness<Reassembler>::execute(ReassemblerTestStep{test}); // 将 ByteStream 测试步骤转换为 Reassembler 测试步骤并执行
    }

    // 允许使用基类的 execute 方法
    using TestHarness<Reassembler>::execute;
};

// BytesPending 结构体，用于检查 Reassembler 中的待处理字节数
struct BytesPending : public ConstExpectNumber<Reassembler, uint64_t>
{
    using ConstExpectNumber::ConstExpectNumber; // 继承构造函数

    // 返回检查的名称
    std::string name() const override { return "bytes_pending"; }

    // 返回 Reassembler 中的待处理字节数
    uint64_t value(const Reassembler &r) const override { return r.bytes_pending(); }
};

// Insert 结构体，表示插入操作
struct Insert : public Action<Reassembler>
{
    std::string data_;         // 要插入的数据
    uint64_t first_index_;     // 插入的起始索引
    bool is_last_substring_{}; // 标记是否为最后一个子字符串

    // 构造函数，初始化数据和起始索引
    Insert(std::string data, uint64_t first_index) : data_(move(data)), first_index_(first_index) {}

    // 设置是否为最后一个子字符串
    Insert &is_last(bool status = true)
    {
        is_last_substring_ = status; // 更新标记
        return *this;                // 返回自身引用
    }

    // 返回插入操作的描述
    std::string description() const override
    {
        std::ostringstream ss;                                                          // 创建字符串流
        ss << "insert \"" << Printer::prettify(data_) << "\" @ index " << first_index_; // 格式化描述
        if (is_last_substring_)                                                         // 如果是最后一个子字符串
        {
            ss << " [last substring]"; // 添加标记
        }
        return ss.str(); // 返回描述字符串
    }

    // 执行插入操作
    void execute(Reassembler &r) const override { r.insert(first_index_, data_, is_last_substring_); }
};

#endif // 结束头文件保护
