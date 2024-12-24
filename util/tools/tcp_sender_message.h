#ifndef TCP_SENDER_MESSAGE_H
#define TCP_SENDER_MESSAGE_H

#include <string>               // 引入字符串类
#include "wrapping_integers.h"  // 引入自定义的整数包装类，用于处理序列号

/*
 * TCPSenderMessage 结构体包含从 TCP 发送方发送到接收方的信息。
 *
 * 它包含五个字段：
 *
 * 1) 序列号 (seqno): 段的起始序列号。如果设置了 SYN 标志，则这是 SYN 标志的序列号。
 *    否则，它是有效载荷的起始序列号。
 *
 * 2) SYN 标志: 如果设置，表示该段是字节流的开始，seqno 字段包含初始序列号 (ISN) -- 零点。
 *
 * 3) 有效载荷 (payload): 字节流的子字符串（可能为空）。
 *
 * 4) FIN 标志: 如果设置，有效载荷表示字节流的结束。
 *
 * 5) RST (重置) 标志: 如果设置，表示流发生错误，连接应被中止。
 */

// TCPSenderMessage 结构体定义
struct TCPSenderMessage
{
    Wrap32 seqno{0}; // 段的起始序列号，使用 Wrap32 类型处理序列号

    bool SYN{};            // SYN 标志，表示是否为连接的开始
    std::string payload{}; // 有效载荷，表示要发送的数据
    bool FIN{};            // FIN 标志，表示是否为连接的结束

    bool RST{}; // RST 标志，表示连接是否应被重置

    // 计算该段使用的序列号数量
    size_t sequence_length() const
    {
        return SYN + payload.size() + FIN; // 返回序列号的总长度
    }
};

#endif // TCP_SENDER_MESSAGE_H
