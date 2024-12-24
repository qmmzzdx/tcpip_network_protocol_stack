#ifndef TCP_RECEIVER_MESSAGE_H
#define TCP_RECEIVER_MESSAGE_H

#include <optional>             // 引入 std::optional，用于表示可选值
#include "wrapping_integers.h"  // 引入自定义的整数包装类，用于处理序列号

/*
 * TCPReceiverMessage 结构体包含从 TCP 接收方发送到发送方的信息。
 *
 * 它包含三个字段：
 *
 * 1) 确认号 (ackno): TCP 接收方所需的下一个序列号。
 *    这是一个可选字段，如果 TCPReceiver 尚未接收到初始序列号，则为空。
 *
 * 2) 窗口大小 (window_size): TCP 接收方希望接收的序列号的数量，
 *    从 ackno 开始（如果存在）。最大值为 65,535 (UINT16_MAX 来自 <cstdint> 头文件)。
 *
 * 3) RST (重置) 标志: 如果设置，表示流发生错误，连接应被中止。
 */

// TCPReceiverMessage 结构体定义
struct TCPReceiverMessage
{
    std::optional<Wrap32> ackno{}; // 确认号，表示下一个期望的序列号，使用 std::optional 表示可选性
    uint16_t window_size{};        // 窗口大小，表示接收方希望接收的序列号数量
    bool RST{};                    // RST 标志，表示连接是否应被重置
};

#endif // TCP_RECEIVER_MESSAGE_H
