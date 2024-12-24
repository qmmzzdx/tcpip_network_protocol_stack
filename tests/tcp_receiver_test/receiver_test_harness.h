#ifndef RECEIVER_TEST_HARNESS_H
#define RECEIVER_TEST_HARNESS_H

#include <optional>                   // 引入可选类型库，提供 std::optional
#include <sstream>                    // 引入字符串流库，用于字符串操作
#include <utility>                    // 引入实用工具库，提供 std::move 等功能
#include "common.h"                   // 引入公共定义和工具
#include "reassembler_test_harness.h" // 引入重组器测试工具的定义
#include "tcp_receiver.h"             // 引入 TCP 接收器的定义
#include "tcp_receiver_message.h"     // 引入 TCP 接收器消息的定义

// 直接重组器测试模板
template <std::derived_from<TestStep<Reassembler>> T>
struct DirectReassemblerTest : public TestStep<TCPReceiver>
{
    T step_; // 存储重组器测试步骤

    // 构造函数，接受一个重组器测试步骤
    template <typename... Targs>
    explicit DirectReassemblerTest(T sr_test_step) : TestStep<TCPReceiver>(), step_(std::move(sr_test_step))
    {
    }

    // 返回步骤的字符串表示
    std::string str() const override { return step_.str(); }
    // 返回步骤的颜色
    uint8_t color() const override { return step_.color(); }
    // 执行步骤，传入 TCP 接收器的引用
    void execute(TCPReceiver &sr) const override { step_.execute(sr.reassembler()); }
};

// 直接字节流测试模板
template <std::derived_from<TestStep<ByteStream>> T>
struct DirectByteStreamTest : public TestStep<TCPReceiver>
{
    T step_; // 存储字节流测试步骤

    // 构造函数，接受一个字节流测试步骤
    template <typename... Targs>
    explicit DirectByteStreamTest(T sr_test_step) : TestStep<TCPReceiver>(), step_(std::move(sr_test_step))
    {
    }

    // 返回步骤的字符串表示
    std::string str() const override { return step_.str(); }
    // 返回步骤的颜色
    uint8_t color() const override { return step_.color(); }
    // 执行步骤，传入 TCP 接收器的引用
    void execute(TCPReceiver &sr) const override { step_.execute(sr.reader()); }
};

// TCP 接收器测试工具类
class TCPReceiverTestHarness : public TestHarness<TCPReceiver>
{
public:
    // 构造函数，接受测试名称和容量
    TCPReceiverTestHarness(std::string test_name, uint64_t capacity)
        : TestHarness(move(test_name),
                      "capacity=" + std::to_string(capacity),
                      {TCPReceiver{Reassembler{ByteStream{capacity}}}})
    {
    }

    // 执行重组器测试步骤
    template <std::derived_from<TestStep<Reassembler>> T>
    void execute(const T &test)
    {
        TestHarness<TCPReceiver>::execute(DirectReassemblerTest{test});
    }

    // 执行字节流测试步骤
    template <std::derived_from<TestStep<ByteStream>> T>
    void execute(const T &test)
    {
        TestHarness<TCPReceiver>::execute(DirectByteStreamTest{test});
    }

    // 允许使用基类的 execute 方法
    using TestHarness<TCPReceiver>::execute;
};

// 期望窗口大小
struct ExpectWindow : public ExpectNumber<TCPReceiver, uint16_t>
{
    using ExpectNumber::ExpectNumber;                                                // 继承构造函数
    std::string name() const override { return "window_size"; }                      // 返回名称
    uint16_t value(TCPReceiver &rs) const override { return rs.send().window_size; } // 返回窗口大小
};

// 期望确认号
struct ExpectAckno : public ExpectNumber<TCPReceiver, std::optional<Wrap32>>
{
    using ExpectNumber::ExpectNumber;                                                       // 继承构造函数
    std::string name() const override { return "ackno"; }                                   // 返回名称
    std::optional<Wrap32> value(TCPReceiver &rs) const override { return rs.send().ackno; } // 返回确认号
};

// 期望重置标志
struct ExpectReset : public ExpectBool<TCPReceiver>
{
    using ExpectBool::ExpectBool;                       // 继承构造函数
    std::string name() const override { return "RST"; } // 返回名称

    bool value(TCPReceiver &rs) const override { return rs.send().RST; } // 返回重置标志
};

// 期望确认号在某个范围内
struct ExpectAcknoBetween : public Expectation<TCPReceiver>
{
    Wrap32 isn_;                                                                    // 初始序列号
    uint64_t checkpoint_;                                                           // 检查点
    uint64_t min_, max_;                                                            // 最小和最大值
    ExpectAcknoBetween(Wrap32 isn, uint64_t checkpoint, uint64_t min, uint64_t max) // 构造函数
        : isn_(isn), checkpoint_(checkpoint), min_(min), max_(max)
    {
    }

    // 返回描述
    std::string description() const override
    {
        return "ackno unwraps to between " + to_string(min_) + " and " + to_string(max_);
    }

    // 执行检查
    void execute(TCPReceiver &rs) const override
    {
        auto ackno = rs.send().ackno; // 获取确认号
        if (not ackno.has_value())    // 检查确认号是否存在
        {
            throw ExpectationViolation("TCPReceiver did not have ackno when expected");
        }
        const uint64_t ackno_absolute = ackno.value().unwrap(isn_, checkpoint_); // 解包确认号

        // 检查确认号是否在范围内
        if (ackno_absolute < min_ or ackno_absolute > max_)
        {
            throw ExpectationViolation("ackno outside expected range");
        }
    }
};

// 检查确认号是否存在
struct HasAckno : public ExpectBool<TCPReceiver>
{
    using ExpectBool::ExpectBool;                                                      // 继承构造函数
    std::string name() const override { return "ackno.has_value()"; }                  // 返回名称
    bool value(TCPReceiver &rs) const override { return rs.send().ackno.has_value(); } // 返回确认号是否存在
};

// 数据段到达的动作
struct SegmentArrives : public Action<TCPReceiver>
{
    TCPSenderMessage msg_{};        // TCP 发送者消息
    HasAckno ackno_expected_{true}; // 期望确认号

    // 设置 SYN 标志
    SegmentArrives &with_syn()
    {
        msg_.SYN = true;
        return *this;
    }

    // 设置 FIN 标志
    SegmentArrives &with_fin()
    {
        msg_.FIN = true;
        return *this;
    }

    // 设置 RST 标志
    SegmentArrives &with_rst()
    {
        msg_.RST = true;
        return *this;
    }

    // 设置序列号
    SegmentArrives &with_seqno(Wrap32 seqno_)
    {
        msg_.seqno = seqno_;
        return *this;
    }

    // 设置序列号（uint32_t 版本）
    SegmentArrives &with_seqno(uint32_t seqno_) { return with_seqno(Wrap32{seqno_}); }

    // 设置数据负载
    SegmentArrives &with_data(std::string data)
    {
        msg_.payload = move(data);
        return *this;
    }

    // 不期望确认号
    SegmentArrives &without_ackno()
    {
        ackno_expected_ = HasAckno{false};
        return *this;
    }

    // 执行接收段的操作
    void execute(TCPReceiver &rs) const override
    {
        rs.receive(msg_);            // 接收消息
        ackno_expected_.execute(rs); // 执行确认号检查
    }

    // 返回描述
    std::string description() const override
    {
        std::ostringstream ss;
        ss << "receive segment: (";
        ss << "seqno=" << msg_.seqno; // 序列号
        if (msg_.SYN)
        {
            ss << " +SYN"; // 如果有 SYN 标志
        }
        if (!msg_.payload.empty())
        {
            ss << " payload=\"" << Printer::prettify(msg_.payload) << "\""; // 如果有负载
        }
        if (msg_.FIN)
        {
            ss << " +FIN"; // 如果有 FIN 标志
        }
        ss << ")";

        if (ackno_expected_.value_)
        {
            ss << " with ackno expected"; // 如果期望确认号
        }
        else
        {
            ss << " with ackno not expected"; // 如果不期望确认号
        }
        return ss.str(); // 返回描述字符串
    }
};

#endif
