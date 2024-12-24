#ifndef BYTE_STREAM_TEST_HARNESS_H
#define BYTE_STREAM_TEST_HARNESS_H

#include <utility>                    // 引入实用工具库，提供 std::move 等功能
#include <concepts>                   // 引入概念库，用于类型约束
#include <optional>                   // 引入可选库，提供 std::optional 类型
#include "common.h"                   // 引入通用头文件，包含一些常用的定义
#include "byte_stream.h"              // 引入自定义的 ByteStream 类头文件

// 静态断言，确保 Reader 和 Writer 的大小与 ByteStream 相同
static_assert(sizeof(Reader) == sizeof(ByteStream),
              "Please add member variables to the ByteStream base, not the ByteStream Reader.");

static_assert(sizeof(Writer) == sizeof(ByteStream),
              "Please add member variables to the ByteStream base, not the ByteStream Writer.");

// ByteStreamTestHarness 类用于测试 ByteStream
class ByteStreamTestHarness : public TestHarness<ByteStream>
{
public:
    // 构造函数，初始化测试工具
    ByteStreamTestHarness(std::string test_name, uint64_t capacity)
        : TestHarness(move(test_name), "capacity=" + std::to_string(capacity), ByteStream{capacity})
    {
    }

    // 获取当前可查看的字节数
    size_t peek_size() { return object().reader().peek().size(); }
};

// Push 操作，用于将数据推送到 ByteStream
struct Push : public Action<ByteStream>
{
    std::string data_; // 要推送的数据

    explicit Push(std::string data) : data_(move(data)) {} // 构造函数
    std::string description() const override { return "push \"" + Printer::prettify(data_) + "\" to the stream"; }
    void execute(ByteStream &bs) const override { bs.writer().push(data_); } // 执行推送操作
};

// Close 操作，用于关闭 ByteStream 的写入器
struct Close : public Action<ByteStream>
{
    std::string description() const override { return "close"; }
    void execute(ByteStream &bs) const override { bs.writer().close(); } // 执行关闭操作
};

// SetError 操作，用于设置 ByteStream 的错误状态
struct SetError : public Action<ByteStream>
{
    std::string description() const override { return "set_error"; }
    void execute(ByteStream &bs) const override { bs.set_error(); } // 执行设置错误操作
};

// Pop 操作，用于从 ByteStream 中弹出数据
struct Pop : public Action<ByteStream>
{
    size_t len_; // 要弹出的字节数

    explicit Pop(size_t len) : len_(len) {} // 构造函数
    std::string description() const override { return "pop( " + std::to_string(len_) + " )"; }
    void execute(ByteStream &bs) const override { bs.reader().pop(len_); } // 执行弹出操作
};

// Peek 期望，用于验证预览的输出
struct Peek : public Expectation<ByteStream>
{
    std::string output_; // 期望的输出

    explicit Peek(std::string output) : output_(move(output)) {} // 构造函数

    std::string description() const override { return "peeking produces \"" + Printer::prettify(output_) + "\""; }

    void execute(ByteStream &bs) const override
    {
        const ByteStream orig = bs; // 保存原始 ByteStream
        std::string got; // 实际获取的输出

        // 循环读取缓冲区中的数据
        while (bs.reader().bytes_buffered())
        {
            auto peeked = bs.reader().peek(); // 预览数据
            if (peeked.empty())
            {
                throw ExpectationViolation{"Reader::peek() returned empty string_view"}; // 抛出异常
            }
            got += peeked; // 将预览的数据添加到实际输出中
            bs.reader().pop(peeked.size()); // 从缓冲区中移除预览的数据
        }

        // 验证预览的输出是否与期望一致
        if (got != output_)
        {
            throw ExpectationViolation{"Expected \"" + Printer::prettify(output_) + "\" in buffer, " + " but found \"" + Printer::prettify(got) + "\""};
        }

        bs = orig; // 恢复原始 ByteStream
    }
};

// PeekOnce 期望，验证预览的输出是否完全匹配
struct PeekOnce : public Peek
{
    using Peek::Peek; // 继承 Peek 的构造函数

    std::string description() const override
    {
        return "peek() gives exactly \"" + Printer::prettify(output_) + "\"";
    }

    void execute(ByteStream &bs) const override
    {
        auto peeked = bs.reader().peek(); // 预览数据
        if (peeked != output_)
        {
            throw ExpectationViolation{"Expected exactly \"" + Printer::prettify(output_) + "\" at front of stream, " + "but found \"" + Printer::prettify(peeked) + "\""};
        }
    }
};

// IsClosed 期望，验证 ByteStream 的写入器是否关闭
struct IsClosed : public ConstExpectBool<ByteStream>
{
    using ConstExpectBool::ConstExpectBool; // 继承构造函数
    std::string name() const override { return "is_closed"; }
    bool value(const ByteStream &bs) const override { return bs.writer().is_closed(); } // 返回写入器的关闭状态
};

// IsFinished 期望，验证 ByteStream 的读取器是否完成
struct IsFinished : public ConstExpectBool<ByteStream>
{
    using ConstExpectBool::ConstExpectBool; // 继承构造函数
    std::string name() const override { return "is_finished"; }
    bool value(const ByteStream &bs) const override { return bs.reader().is_finished(); } // 返回读取器的完成状态
};

// HasError 期望，验证 ByteStream 是否有错误
struct HasError : public ConstExpectBool<ByteStream>
{
    using ConstExpectBool::ConstExpectBool; // 继承构造函数
    std::string name() const override { return "has_error"; }
    bool value(const ByteStream &bs) const override { return bs.has_error(); } // 返回错误状态
};

// BytesBuffered 期望，获取缓冲区中的字节数
struct BytesBuffered : public ConstExpectNumber<ByteStream, uint64_t>
{
    using ConstExpectNumber::ConstExpectNumber; // 继承构造函数
    std::string name() const override { return "bytes_buffered"; }
    size_t value(const ByteStream &bs) const override { return bs.reader().bytes_buffered(); } // 返回缓冲区字节数
};

// BufferEmpty 期望，验证缓冲区是否为空
struct BufferEmpty : public ExpectBool<ByteStream>
{
    using ExpectBool::ExpectBool; // 继承构造函数
    std::string name() const override { return "[buffer is empty]"; }
    bool value(ByteStream &bs) const override { return bs.reader().bytes_buffered() == 0; } // 返回缓冲区是否为空
};

// AvailableCapacity 期望，获取可用容量
struct AvailableCapacity : public ExpectNumber<ByteStream, uint64_t>
{
    using ExpectNumber::ExpectNumber; // 继承构造函数
    std::string name() const override { return "available_capacity"; }
    size_t value(ByteStream &bs) const override { return bs.writer().available_capacity(); } // 返回可用容量
};

// BytesPushed 期望，获取已推送的字节数
struct BytesPushed : public ExpectNumber<ByteStream, uint64_t>
{
    using ExpectNumber::ExpectNumber; // 继承构造函数
    std::string name() const override { return "bytes_pushed"; }
    size_t value(ByteStream &bs) const override { return bs.writer().bytes_pushed(); } // 返回已推送字节数
};

// BytesPopped 期望，获取已弹出的字节数
struct BytesPopped : public ExpectNumber<ByteStream, uint64_t>
{
    using ExpectNumber::ExpectNumber; // 继承构造函数
    std::string name() const override { return "bytes_popped"; }
    size_t value(ByteStream &bs) const override { return bs.reader().bytes_popped(); } // 返回已弹出字节数
};

// ReadAll 期望，验证读取所有数据后缓冲区是否为空
struct ReadAll : public Expectation<ByteStream>
{
    std::string output_; // 期望的输出
    BufferEmpty empty_{true}; // 用于验证缓冲区是否为空

    explicit ReadAll(std::string output) : output_(move(output)) {} // 构造函数

    std::string description() const override
    {
        if (output_.empty())
        {
            return empty_.description(); // 如果输出为空，返回缓冲区为空的描述
        }
        return "reading \"" + Printer::prettify(output_) + "\" leaves buffer empty"; // 返回读取描述
    }

    void execute(ByteStream &bs) const override
    {
        std::string got; // 实际读取的输出
        read(bs.reader(), output_.size(), got); // 读取数据
        if (got != output_)
        {
            throw ExpectationViolation{"Expected to read \"" + Printer::prettify(output_) + "\", but found \"" + Printer::prettify(got) + "\""}; // 验证读取结果
        }
        empty_.execute(bs); // 验证缓冲区是否为空
    }
};

#endif
