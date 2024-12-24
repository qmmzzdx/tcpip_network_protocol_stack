#include <cstddef>
#include <cstdint>
#include <utility>
#include <string_view>
#include <algorithm>
#include "tcp_sender.h"
#include "tcp_config.h"
#include "wrapping_integers.h"

// 返回当前未确认的序列号数量
uint64_t TCPSender::sequence_numbers_in_flight() const
{
    return total_outstandings_;
}

// 返回连续重传的次数
uint64_t TCPSender::consecutive_retransmissions() const
{
    return total_retransmissions_;
}

// 负责将数据推送到网络中
void TCPSender::push(const TransmitFunction& transmit)
{
    // 确定最大窗口大小，如果窗口大小为0，则设为1以确保至少能发送一个字节
    uint64_t max_wdsize = (wdsize_ > 0 ? wdsize_ : 1);

    // 只要窗口允许并且没有发送FIN，就继续发送数据
    while (max_wdsize > total_outstandings_ && !FIN_flag_)
    {
        // 创建一个空的TCP消息
        TCPSenderMessage msg = make_empty_message();

        // 如果还没有发送SYN，则发送SYN
        if (!SYN_flag_)
        {
            msg.SYN = true;
            SYN_flag_ = true;
        }

        // 计算剩余的窗口大小
        uint64_t remains = max_wdsize - total_outstandings_;
        // 从输入流中读取数据，读取的大小不能超过最大负载和剩余窗口大小
        uint64_t payload_size = std::min(TCPConfig::MAX_PAYLOAD_SIZE, remains - msg.sequence_length());
        read(input_.reader(), payload_size, msg.payload);

        // 如果可以发送FIN并且输入流已结束，则发送FIN
        if (!FIN_flag_ && remains > msg.sequence_length() && reader().is_finished())
        {
            msg.FIN = true;
            FIN_flag_ = true;
        }

        // 如果消息长度为0，退出循环
        if (msg.sequence_length() == 0)
        {
            break;
        }

        // 发送消息
        transmit(msg);

        // 如果计时器未激活，则启动计时器
        if (!timer_.is_active())
        {
            timer_.start();
        }

        // 更新下一个绝对序列号和未完成的数量
        next_absseq_ += msg.sequence_length();
        total_outstandings_ += msg.sequence_length();

        // 将消息加入队列
        qmesg_.emplace(std::move(msg));
    }
}

// 创建一个空的TCPSenderMessage
TCPSenderMessage TCPSender::make_empty_message() const
{
    // 使用当前的绝对序列号和初始序列号来创建一个空消息
    return {Wrap32::wrap(next_absseq_, isn_), false, {}, false, input_.has_error()};
}

// 处理接收到的TCPReceiverMessage
void TCPSender::receive(const TCPReceiverMessage& msg)
{
    // 如果输入流有错误，直接返回
    if (input_.has_error())
    {
        return;
    }

    // 如果接收到RST标志，设置错误并返回
    if (msg.RST)
    {
        input_.set_error();
        return;
    }

    // 更新窗口大小
    wdsize_ = msg.window_size;

    // 如果没有ackno，直接返回
    if (!msg.ackno.has_value())
    {
        return;
    }

    // 计算接收到的ack的绝对序列号
    const uint64_t recv_ack_absseq = msg.ackno->unwrap(isn_, next_absseq_);

    // 如果接收到的ack序列号大于下一个序列号，直接返回
    if (recv_ack_absseq > next_absseq_)
    {
        return;
    }

    bool has_ackno_flag = false;

    // 处理确认的消息
    while (!qmesg_.empty())
    {
        const auto& message = qmesg_.front();

        // 如果消息的序列号超出接收到的ack序列号，停止处理
        if (ack_absseq_ + message.sequence_length() > recv_ack_absseq)
        {
            break;
        }

        // 更新确认的序列号和未完成的数量
        has_ackno_flag = true;
        ack_absseq_ += message.sequence_length();
        total_outstandings_ -= message.sequence_length();
        qmesg_.pop();
    }

    // 如果有新的ack，重置重传计数器和计时器
    if (has_ackno_flag)
    {
        total_retransmissions_ = 0;
        timer_.reload(initial_RTO_ms_);
        qmesg_.empty() ? timer_.stop() : timer_.start();
    }
}

// 处理计时器的tick事件
void TCPSender::tick(uint64_t ms_since_last_tick, const TransmitFunction& transmit)
{
    // 更新计时器，如果计时器过期且消息队列不为空，进行重传
    if (timer_.tick(ms_since_last_tick).is_expired() && !qmesg_.empty())
    {
        // 重传队列中的第一个消息
        transmit(qmesg_.front());

        // 如果窗口大小不为0，则进行指数退避
        if (wdsize_ != 0)
        {
            ++total_retransmissions_;
            timer_.exponential_backoff();
        }

        // 重置计时器
        timer_.reset();
    }
}
