#include <cstddef>              // 包含 size_t 的定义
#include "tcp_segment.h"        // 包含 TCPSegment 类的定义
#include "checksum.h"           // 包含校验和计算的定义
#include "wrapping_integers.h"  // 包含包装整数的定义

static constexpr uint32_t TCPHeaderMinLen = 5; // TCP 头部的最小长度（以 32 位字为单位）

// 解析 TCP 段的函数
void TCPSegment::parse(Parser &parser, uint32_t datagram_layer_pseudo_checksum)
{
    /* 验证校验和 */
    InternetChecksum check{datagram_layer_pseudo_checksum}; // 创建校验和对象
    check.add(parser.buffer());                             // 将解析器的缓冲区内容添加到校验和计算中
    if (check.value())
    {                       // 如果校验和不正确
        parser.set_error(); // 设置解析器错误状态
        return;             // 退出解析函数
    }

    uint32_t raw32{}; // 用于存储 32 位原始数据
    uint16_t raw16{}; // 用于存储 16 位原始数据
    uint8_t octet{};  // 用于存储 8 位数据

    // 解析源端口和目标端口
    parser.integer(udinfo.src_port);
    parser.integer(udinfo.dst_port);

    // 解析序列号
    parser.integer(raw32);
    message.sender.seqno = Wrap32{raw32}; // 将原始序列号包装为 Wrap32 类型

    // 解析确认号
    parser.integer(raw32);
    message.receiver.ackno = Wrap32{raw32}; // 将原始确认号包装为 Wrap32 类型

    // 解析数据偏移
    parser.integer(octet);
    const uint8_t data_offset = octet >> 4; // 数据偏移量（以 32 位字为单位）

    // 解析标志位
    parser.integer(octet); // 解析标志位
    if (!(octet & 0b0001'0000))
    {                                   // 检查 ACK 标志位
        message.receiver.ackno.reset(); // 如果没有 ACK，重置确认号
    }

    // 解析 RST、SYN 和 FIN 标志
    message.sender.RST = message.receiver.RST = octet & 0b0000'0100; // RST 标志
    message.sender.SYN = octet & 0b0000'0010;                        // SYN 标志
    message.sender.FIN = octet & 0b0000'0001;                        // FIN 标志

    // 解析窗口大小
    parser.integer(message.receiver.window_size);
    parser.integer(udinfo.cksum); // 解析校验和
    parser.integer(raw16);        // 解析紧急指针（未使用）

    // 跳过 TCP 头部中的选项或额外内容
    if (data_offset < TCPHeaderMinLen)
    {                       // 如果数据偏移小于最小长度
        parser.set_error(); // 设置解析器错误状态
    }
    parser.remove_prefix(data_offset * 4 - TCPHeaderMinLen * 4); // 移除多余的前缀

    // 解析剩余的有效载荷
    parser.all_remaining(message.sender.payload);
}

// Wrap32Serializable 类用于序列化 Wrap32 类型
class Wrap32Serializable : public Wrap32
{
public:
    uint32_t raw_value() const { return raw_value_; } // 返回原始值
};

// 序列化 TCP 段的函数
void TCPSegment::serialize(Serializer &serializer) const
{
    // 序列化源端口和目标端口
    serializer.integer(udinfo.src_port);
    serializer.integer(udinfo.dst_port);

    // 序列化序列号
    serializer.integer(Wrap32Serializable{message.sender.seqno}.raw_value());

    // 序列化确认号，若无确认号则使用默认值 0
    serializer.integer(Wrap32Serializable{message.receiver.ackno.value_or(Wrap32{0})}.raw_value());

    // 序列化数据偏移（以 32 位字为单位）
    serializer.integer(uint8_t{TCPHeaderMinLen << 4}); // 数据偏移

    // 计算标志位
    const bool reset = message.sender.RST || message.receiver.RST;                  // 检查 RST 标志
    const uint8_t flags = (message.receiver.ackno.has_value() ? 0b0001'0000U : 0) | // ACK 标志
                          (reset ? 0b0000'0100U : 0) |                              // RST 标志
                          (message.sender.SYN ? 0b0000'0010U : 0) |                 // SYN 标志
                          (message.sender.FIN ? 0b0000'0001U : 0);                  // FIN 标志
    serializer.integer(flags);                                                      // 序列化标志位

    // 序列化窗口大小
    serializer.integer(message.receiver.window_size);
    serializer.integer(udinfo.cksum); // 序列化校验和
    serializer.integer(uint16_t{0});  // 紧急指针（未使用）

    // 序列化发送者的有效载荷
    serializer.buffer(message.sender.payload);
}

// 计算 TCP 段的校验和
void TCPSegment::compute_checksum(uint32_t datagram_layer_pseudo_checksum)
{
    udinfo.cksum = 0; // 重置校验和
    Serializer s;     // 创建序列化器
    serialize(s);     // 序列化当前 TCP 段

    InternetChecksum check{datagram_layer_pseudo_checksum}; // 创建校验和对象
    check.add(s.output());                                  // 将序列化后的数据添加到校验和计算中
    udinfo.cksum = check.value();                           // 计算并存储校验和
}
