#ifndef TCP_SEGMENT_H
#define TCP_SEGMENT_H

#include "parser.h"               // 包含解析器的定义
#include "tcp_sender_message.h"   // 包含 TCP 发送者消息的定义
#include "tcp_receiver_message.h" // 包含 TCP 接收者消息的定义
#include "udinfo.h"               // 包含用户数据报信息的定义

// TCPMessage 结构体用于封装 TCP 发送者和接收者的消息
struct TCPMessage
{
    TCPSenderMessage sender{};     // TCP 发送者消息，默认初始化
    TCPReceiverMessage receiver{}; // TCP 接收者消息，默认初始化
};

// TCPSegment 结构体表示一个 TCP 段
struct TCPSegment
{
    TCPMessage message{};      // TCP 消息，包含发送者和接收者的信息
    UserDatagramInfo udinfo{}; // 用户数据报信息，包含与数据报相关的元数据

    // 解析 TCP 段的函数，使用给定的解析器和伪校验和
    void parse(Parser &parser, uint32_t datagram_layer_pseudo_checksum);

    // 序列化 TCP 段的函数，将其转换为可传输的格式
    void serialize(Serializer &serializer) const;

    // 计算 TCP 段的校验和，使用给定的伪校验和
    void compute_checksum(uint32_t datagram_layer_pseudo_checksum);
};

#endif
