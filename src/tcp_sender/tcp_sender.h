#ifndef TCP_SENDER_H
#define TCP_SENDER_H

#include <cstdint>
#include <queue>
#include <functional>
#include "byte_stream.h"
#include "tcp_receiver_message.h"
#include "tcp_sender_message.h"

// 重传计时器类，用于管理TCP段的重传超时
class RetransmissionTimer
{
public:
    // 构造函数，初始化重传超时（RTO）
    explicit RetransmissionTimer(uint64_t initial_RTO_ms) : rto_ms_(initial_RTO_ms) {}
    // 检查计时器是否处于活动状态
    bool is_active() const { return is_active_; }
    // 检查计时器是否已过期
    bool is_expired() const { return is_active_ && time_ms_ >= rto_ms_; }
    // 重置计时器
    void reset() { time_ms_ = 0; }
    // 指数退避，翻倍RTO
    void exponential_backoff() { rto_ms_ <<= 1; }
    // 重新加载初始RTO并重置计时器
    void reload(uint64_t initial_RTO_ms) { rto_ms_ = initial_RTO_ms, reset(); }
    // 启动计时器
    void start() { is_active_ = true, reset(); }
    // 停止计时器
    void stop() { is_active_ = false, reset(); }
    // 更新计时器，增加经过的时间
    RetransmissionTimer &tick(uint64_t ms_since_last_tick)
    {
        time_ms_ += is_active_ ? ms_since_last_tick : 0;
        return *this;
    }

private:
    bool is_active_{};   // 计时器是否处于活动状态
    uint64_t rto_ms_{};  // 当前RTO值
    uint64_t time_ms_{}; // 计时器累计的时间
};

// TCP发送器类，负责管理TCP段的发送和重传
class TCPSender
{
public:
    /* 构造函数，使用给定的默认重传超时和可能的初始序列号(ISN) */
    TCPSender(ByteStream &&input, Wrap32 isn, uint64_t initial_RTO_ms) : input_(std::move(input)), isn_(isn), initial_RTO_ms_(initial_RTO_ms), timer_(initial_RTO_ms) {}

    /* 生成一个空的TCPSenderMessage */
    TCPSenderMessage make_empty_message() const;

    /* 接收并处理来自对等方接收器的TCPReceiverMessage */
    void receive(const TCPReceiverMessage &msg);

    /* `transmit`函数的类型，push和tick方法可以使用它来发送消息 */
    using TransmitFunction = std::function<void(const TCPSenderMessage &)>;

    /* 从输出流中推送字节 */
    void push(const TransmitFunction &transmit);

    /* 自上次调用tick()方法以来，时间已过去给定的毫秒数 */
    void tick(uint64_t ms_since_last_tick, const TransmitFunction &transmit);

    // 访问器
    uint64_t sequence_numbers_in_flight() const;  // 有多少序列号未完成
    uint64_t consecutive_retransmissions() const; // 已发生多少次连续的重传
    Writer &writer() { return input_.writer(); }
    const Writer &writer() const { return input_.writer(); }

    // 访问输入流读取器，但仅限于const（不能从外部读取）
    const Reader &reader() const { return input_.reader(); }

private:
    ByteStream input_;        // 输入字节流
    Wrap32 isn_;              // 初始序列号
    uint64_t initial_RTO_ms_; // 初始重传超时

    RetransmissionTimer timer_;        // 重传计时器
    bool SYN_flag_{};                  // SYN标志
    bool FIN_flag_{};                  // FIN标志
    uint64_t total_outstandings_{};    // 总未完成的段数
    uint64_t total_retransmissions_{}; // 总重传次数

    uint16_t wdsize_{1};                   // 窗口大小
    uint64_t next_absseq_{};               // 下一个绝对序列号
    uint64_t ack_absseq_{};                // 确认的绝对序列号
    std::queue<TCPSenderMessage> qmesg_{}; // 消息队列，存储待发送的TCP段
};

#endif
