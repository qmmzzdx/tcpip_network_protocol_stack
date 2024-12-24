#ifndef SENDER_TEST_HARNESS_H
#define SENDER_TEST_HARNESS_H

#include <optional>
#include <queue>
#include <sstream>
#include <utility>
#include "common.h"
#include "tcp_config.h"
#include "tcp_receiver_message.h"
#include "tcp_segment.h"
#include "tcp_sender.h"
#include "wrapping_integers.h"

// 默认测试窗口大小
const unsigned int DEFAULT_TEST_WINDOW = 137;

// 结构体，用于封装 TCPSender 和输出消息队列
struct SenderAndOutput
{
    TCPSender sender;                      // TCP 发送器实例
    std::queue<TCPSenderMessage> output{}; // 存储发送的消息

    // 创建一个传输函数，用于将消息推入输出队列
    auto make_transmit()
    {
        return [&](const TCPSenderMessage &x) { output.push(x); }; // 将消息 x 推入输出队列
    }
};

// 将 TCPSenderMessage 转换为字符串的辅助函数
inline std::string to_string(const TCPSenderMessage &msg)
{
    std::ostringstream ost;
    ost << "(";
    ost << "seqno=" << msg.seqno; // 序列号
    if (msg.SYN)
    {
        ost << " +SYN"; // 如果有 SYN 标志
    }
    if (not msg.payload.empty())
    {
        ost << " payload=\"" << Printer::prettify(msg.payload) << "\""; // 如果有有效负载
    }
    if (msg.FIN)
    {
        ost << " +FIN"; // 如果有 FIN 标志
    }
    ost << ")";
    return ost.str(); // 返回格式化的字符串
}

// 期望序列号的结构体
struct ExpectSeqno : public ExpectNumber<SenderAndOutput, Wrap32>
{
    using ExpectNumber::ExpectNumber;
    std::string name() const override { return "make_empty_message().seqno"; }

    // 获取期望的序列号
    Wrap32 value(SenderAndOutput &ss) const override
    {
        auto seg = ss.sender.make_empty_message(); // 创建一个空消息
        if (seg.sequence_length())
        {
            throw ExpectationViolation("TCPSender::make_empty_message() returned non-empty message");
        }
        return seg.seqno; // 返回序列号
    }
};

// 期望重置标志的结构体
struct ExpectReset : public ExpectBool<SenderAndOutput>
{
    using ExpectBool::ExpectBool;
    std::string name() const override { return "make_empty_message().RST"; }

    // 检查是否有 RST 标志
    bool value(SenderAndOutput &ss) const override { return ss.sender.make_empty_message().RST; }
};

// 期望在飞行中的序列号数量
struct ExpectSeqnosInFlight : public ExpectNumber<SenderAndOutput, uint64_t>
{
    using ExpectNumber::ExpectNumber;
    std::string name() const override { return "sequence_numbers_in_flight"; }
    uint64_t value(SenderAndOutput &ss) const override { return ss.sender.sequence_numbers_in_flight(); }
};

// 期望连续重传次数
struct ExpectConsecutiveRetransmissions : public ExpectNumber<SenderAndOutput, uint64_t>
{
    using ExpectNumber::ExpectNumber;
    std::string name() const override { return "consecutive_retransmissions"; }
    uint64_t value(SenderAndOutput &ss) const override { return ss.sender.consecutive_retransmissions(); }
};

// 期望没有发送的消息
struct ExpectNoSegment : public Expectation<SenderAndOutput>
{
    std::string description() const override { return "nothing to send"; }
    void execute(SenderAndOutput &ss) const override
    {
        if (not ss.output.empty())
        {
            throw ExpectationViolation{"TCPSender sent an unexpected segment: " + to_string(ss.output.front())};
        }
    }
};

// 设置错误的操作
struct SetError : public Action<SenderAndOutput>
{
    std::string description() const override { return "set_error"; }
    void execute(SenderAndOutput &ss) const override { ss.sender.writer().set_error(); }
};

// 检查是否有错误的期望
struct HasError : public ExpectBool<SenderAndOutput>
{
    using ExpectBool::ExpectBool;
    std::string name() const override { return "has_error"; }
    bool value(SenderAndOutput &ss) const override { return ss.sender.writer().has_error(); }
};

// 推送数据到发送器的操作
struct Push : public Action<SenderAndOutput>
{
    std::string data_; // 要推送的数据
    bool close_{};     // 是否关闭流

    explicit Push(std::string data = "") : data_(move(data)) {} // 构造函数

    // 描述推送操作
    std::string description() const override
    {
        if (data_.empty() and not close_)
        {
            return "push TCPSender";
        }

        if (data_.empty() and close_)
        {
            return "close stream, then push to TCPSender";
        }

        return "push \"" + Printer::prettify(data_) + "\" to stream" + (close_ ? ", close it" : "") + ", then push to TCPSender";
    }

    // 执行推送操作
    void execute(SenderAndOutput &ss) const override
    {
        if (not data_.empty())
        {
            ss.sender.writer().push(data_); // 推送数据
        }
        if (close_)
        {
            ss.sender.writer().close(); // 关闭流
        }
        ss.sender.push(ss.make_transmit()); // 将消息推送到发送器
    }

    // 设置关闭流的标志
    Push &with_close()
    {
        close_ = true;
        return *this;
    }
};

// 模拟时间流逝的操作
struct Tick : public Action<SenderAndOutput>
{
    uint64_t ms_;                             // 时间流逝的毫秒数
    std::optional<bool> max_retx_exceeded_{}; // 最大重传次数是否超出

    explicit Tick(uint64_t ms) : ms_(ms) {} // 构造函数

    // 设置最大重传次数超出的标志
    Tick &with_max_retx_exceeded(bool val)
    {
        max_retx_exceeded_ = val;
        return *this;
    }

    // 描述时间流逝操作
    std::string description() const override
    {
        std::ostringstream desc;
        desc << ms_ << " ms pass";
        if (max_retx_exceeded_.has_value())
        {
            desc << " with max_retx_exceeded = " << max_retx_exceeded_.value();
        }
        return desc.str();
    }

    // 执行时间流逝操作
    void execute(SenderAndOutput &ss) const override
    {
        ss.sender.tick(ms_, ss.make_transmit()); // 更新发送器状态
        // 检查最大重传次数是否符合预期
        if (max_retx_exceeded_.has_value() and max_retx_exceeded_ != (ss.sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS))
        {
            std::ostringstream desc;
            desc << "after " << ms_ << " ms passed the TCP Sender reported\n\tconsecutive_retransmissions = "
                 << ss.sender.consecutive_retransmissions() << "\nbut it should have been\n\t";
            if (max_retx_exceeded_.value())
            {
                desc << "greater than ";
            }
            else
            {
                desc << "less than or equal to ";
            }
            desc << TCPConfig::MAX_RETX_ATTEMPTS << "\n";
            throw ExpectationViolation(desc.str());
        }
    }
};

// 接收消息的操作
struct Receive : public Action<SenderAndOutput>
{
    TCPReceiverMessage msg_; // 接收到的消息
    bool push_ = true;       // 是否推送到发送器

    explicit Receive(TCPReceiverMessage msg) : msg_(msg) {} // 构造函数

    // 描述接收操作
    std::string description() const override
    {
        std::ostringstream desc;
        desc << "receive(ack=" << to_string(msg_.ackno) << ", win=" << msg_.window_size << ")";
        if (push_)
        {
            desc << ", then push stream to TCPSender";
        }
        return desc.str();
    }

    // 设置窗口大小
    Receive &with_win(uint16_t win)
    {
        msg_.window_size = win;
        return *this;
    }

    // 执行接收操作
    void execute(SenderAndOutput &ss) const override
    {
        ss.sender.receive(msg_); // 处理接收到的消息
        if (push_)
        {
            ss.sender.push(ss.make_transmit()); // 如果需要，推送到发送器
        }
    }

    // 设置不推送的标志
    Receive &without_push()
    {
        push_ = false;
        return *this;
    }
};

// 确认接收到 ACK 的操作
struct AckReceived : public Receive
{
    explicit AckReceived(Wrap32 ackno) : Receive({ackno, DEFAULT_TEST_WINDOW}) {} // 构造函数
};

// 关闭流的操作
struct Close : public Push
{
    Close() : Push("") { with_close(); } // 构造函数，设置关闭标志
};

// 期望消息的结构体
struct ExpectMessage : public Expectation<SenderAndOutput>
{
    std::optional<bool> syn{};            // SYN 标志
    std::optional<bool> fin{};            // FIN 标志
    std::optional<bool> rst{};            // RST 标志
    std::optional<Wrap32> seqno{};        // 序列号
    std::optional<std::string> data{};    // 负载数据
    std::optional<size_t> payload_size{}; // 负载大小

    // 设置 SYN 标志
    ExpectMessage &with_syn(bool syn_)
    {
        syn = syn_;
        return *this;
    }

    // 设置 FIN 标志
    ExpectMessage &with_fin(bool fin_)
    {
        fin = fin_;
        return *this;
    }

    // 设置无标志
    ExpectMessage &with_no_flags()
    {
        syn = fin = rst = false;
        return *this;
    }

    // 设置 RST 标志
    ExpectMessage &with_rst(bool rst_)
    {
        rst = rst_;
        return *this;
    }

    // 设置序列号
    ExpectMessage &with_seqno(Wrap32 seqno_)
    {
        seqno = seqno_;
        return *this;
    }

    // 设置序列号（uint32_t 类型）
    ExpectMessage &with_seqno(uint32_t seqno_) { return with_seqno(Wrap32{seqno_}); }

    // 设置负载大小
    ExpectMessage &with_payload_size(size_t payload_size_)
    {
        payload_size = payload_size_;
        return *this;
    }

    // 设置负载数据
    ExpectMessage &with_data(std::string data_)
    {
        data = std::move(data_);
        return *this;
    }

    // 描述消息的详细信息
    std::string message_description() const
    {
        std::ostringstream o;
        if (seqno.has_value())
        {
            o << " seqno=" << seqno.value();
        }
        if (syn.has_value())
        {
            o << (syn.value() ? " +SYN" : " (no SYN)");
        }
        if (payload_size.has_value())
        {
            if (payload_size.value())
            {
                o << " payload_len=" << payload_size.value();
            }
            else
            {
                o << " (no payload)";
            }
        }
        if (data.has_value())
        {
            o << " payload=\"" << Printer::prettify(data.value()) << "\"";
        }
        if (fin.has_value())
        {
            o << (fin.value() ? " +FIN" : " (no FIN)");
        }
        if (rst.has_value())
        {
            o << (rst.value() ? " +RST" : " (no RST)");
        }
        return o.str();
    }

    // 描述期望消息
    std::string description() const override { return "message sent with" + message_description(); }

    // 执行期望消息的检查
    void execute(SenderAndOutput &ss) const override
    {
        // 检查负载大小与数据的一致性
        if (payload_size.has_value() and data.has_value() and payload_size.value() != data.value().size())
        {
            throw std::runtime_error("inconsistent test: invalid ExpectMessage");
        }

        // 检查输出队列是否为空
        if (ss.output.empty())
        {
            throw ExpectationViolation("expected a message, but none was sent");
        }

        const TCPSenderMessage &seg = ss.output.front(); // 获取队列中的消息

        // 检查各个标志和属性是否符合预期
        if (syn.has_value() and seg.SYN != syn.value())
        {
            throw ExpectationViolation("SYN flag", syn.value(), seg.SYN);
        }
        if (fin.has_value() and seg.FIN != fin.value())
        {
            throw ExpectationViolation("FIN flag", fin.value(), seg.FIN);
        }
        if (rst.has_value() and seg.RST != rst.value())
        {
            throw ExpectationViolation("RST flag", rst.value(), seg.RST);
        }
        if (seqno.has_value() and seg.seqno != seqno.value())
        {
            throw ExpectationViolation("sequence number", seqno.value(), seg.seqno);
        }
        if (payload_size.has_value() and seg.payload.size() != payload_size.value())
        {
            throw ExpectationViolation("payload_size", payload_size.value(), seg.payload.size());
        }
        if (seg.payload.size() > TCPConfig::MAX_PAYLOAD_SIZE)
        {
            throw ExpectationViolation("payload has length (" + std::to_string(seg.payload.size()) + ") greater than the maximum");
        }
        if (data.has_value() and data.value() != static_cast<std::string>(seg.payload))
        {
            throw ExpectationViolation("Expecting payload of \"" + Printer::prettify(data.value()) + "\", but instead it was \"" + Printer::prettify(seg.payload) + "\"");
        }

        ss.output.pop(); // 从输出队列中移除已检查的消息
    }
};

// TCP 发送器测试工具类
class TCPSenderTestHarness : public TestHarness<SenderAndOutput>
{
public:
    // 构造函数，初始化测试工具
    TCPSenderTestHarness(std::string name, TCPConfig config)
        : TestHarness(move(name),
                      "initial_RTO_ms=" + to_string(config.rt_timeout),
                      {TCPSender{ByteStream{config.send_capacity}, config.isn, config.rt_timeout}})
    {
    }
};

#endif
