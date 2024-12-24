#ifndef TCP_PEER_H
#define TCP_PEER_H

#include <optional>                // 包含 std::optional 的定义
#include <functional>              // 包含 std::function 的定义
#include "tcp_config.h"            // 包含 TCP 配置的定义
#include "tcp_receiver.h"          // 包含 TCP 接收器的定义
#include "tcp_receiver_message.h"  // 包含 TCP 接收器消息的定义
#include "tcp_segment.h"           // 包含 TCP 段的定义
#include "tcp_sender.h"            // 包含 TCP 发送器的定义
#include "tcp_sender_message.h"    // 包含 TCP 发送器消息的定义

// TCPPeer 类用于管理 TCP 连接的发送和接收
class TCPPeer
{
private:
    // 创建发送函数的辅助方法
    auto make_send(const auto &transmit)
    {
        return [&](const TCPSenderMessage &x) { send(x, transmit); }; // 返回一个 lambda 函数，用于发送消息
    }

public:
    // 构造函数，接受 TCP 配置
    explicit TCPPeer(const TCPConfig &cfg) : cfg_(cfg) {}

    // 获取发送器的写入器
    Writer &outbound_writer() { return sender_.writer(); }

    // 获取接收器的读取器
    Reader &inbound_reader() { return receiver_.reader(); }

    // 定义传输函数类型
    using TransmitFunction = std::function<void(TCPMessage)>;

    // 将传输函数推送到发送器
    void push(const TransmitFunction &transmit) { sender_.push(make_send(transmit)); }

    // 处理时间推移，更新发送器状态
    void tick(uint64_t t, const TransmitFunction &transmit)
    {
        cumulative_time_ += t;                // 累加时间
        sender_.tick(t, make_send(transmit)); // 更新发送器状态
    }

    // 检查是否有确认号
    bool has_ackno() const { return receiver_.send().ackno.has_value(); }

    // 检查 TCPPeer 是否处于活动状态
    bool active() const
    {
        const bool any_errors = receiver_.reader().has_error() || sender_.writer().has_error();                                     // 检查是否有错误
        const bool sender_active = sender_.sequence_numbers_in_flight() || !sender_.reader().is_finished();                         // 检查发送器是否活跃
        const bool receiver_active = !receiver_.writer().is_closed();                                                               // 检查接收器是否活跃
        const bool lingering = linger_after_streams_finish_ && (cumulative_time_ < time_of_last_receipt_ + 10UL * cfg_.rt_timeout); // 检查是否处于延迟状态

        return (!any_errors) && (sender_active || receiver_active || lingering); // 返回活动状态
    }

    // 接收 TCP 消息并处理
    void receive(TCPMessage msg, const TransmitFunction &transmit)
    {
        // 如果不活跃，直接返回
        if (!active())
        {
            return;
        }

        // 更新最后接收时间
        time_of_last_receipt_ = cumulative_time_;

        // 检查是否需要发送
        need_send_ |= (msg.sender.sequence_length() > 0);

        const auto our_ackno = receiver_.send().ackno;                                      // 获取当前的确认号
        need_send_ |= (our_ackno.has_value() && msg.sender.seqno + 1 == our_ackno.value()); // 检查是否需要发送

        // 如果接收器的写入器已关闭且发送器的读取器未完成，设置延迟状态
        if (receiver_.writer().is_closed() && !sender_.reader().is_finished())
        {
            linger_after_streams_finish_ = false;
        }

        // 接收消息并处理
        receiver_.receive(std::move(msg.sender)); // 处理发送者消息
        sender_.receive(msg.receiver);            // 处理接收者消息

        push(transmit); // 推送传输函数
        if (need_send_)
        {                                                 // 如果需要发送
            send(sender_.make_empty_message(), transmit); // 发送空消息
        }
    }

    // 获取接收器的引用
    const TCPReceiver &receiver() const { return receiver_; }

    // 获取发送器的引用
    const TCPSender &sender() const { return sender_; }

private:
    TCPConfig cfg_;                                                               // TCP 配置
    TCPSender sender_{ByteStream{cfg_.send_capacity}, cfg_.isn, cfg_.rt_timeout}; // 创建发送器
    TCPReceiver receiver_{Reassembler{ByteStream{cfg_.recv_capacity}}};           // 创建接收器

    bool need_send_{}; // 标记是否需要发送

    // 发送消息的私有方法
    void send(const TCPSenderMessage &sender_message, const TransmitFunction &transmit)
    {
        TCPMessage msg{sender_message, receiver_.send()}; // 创建 TCP 消息
        transmit(std::move(msg));                         // 传输消息
        need_send_ = false;                               // 重置发送标记
    }

    bool linger_after_streams_finish_{true}; // 标记是否在流结束后延迟
    uint64_t cumulative_time_{};             // 累计时间
    uint64_t time_of_last_receipt_{};        // 最后接收时间
};

#endif
