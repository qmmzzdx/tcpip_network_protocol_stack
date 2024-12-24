#include <stdexcept>        // 包含标准异常处理的定义
#include <unistd.h>         // 包含 POSIX 操作系统 API 的定义
#include <utility>          // 包含一些实用工具的定义
#include <arpa/inet.h>      // 包含网络字节序转换的定义
#include "parser.h"         // 包含解析器的定义
#include "ipv4_header.h"    // 包含 IPv4 头部的定义
#include "tcp_over_ip.h"    // 包含 TCPOverIPv4Adapter 的定义
#include "ipv4_datagram.h"  // 包含 IPv4 数据报的定义

std::optional<TCPMessage> TCPOverIPv4Adapter::unwrap_tcp_in_ip(const InternetDatagram &ip_dgram)
{
    // 检查 IPv4 数据报的目标地址是否是本地地址
    // 注意：绑定到地址 "0" (INADDR_ANY) 是有效的，可以从实际联系的地址回复
    if (!listening() && (ip_dgram.header.dst != config().source.ipv4_numeric()))
    {
        return {}; // 如果不在监听状态且目标地址不匹配，返回空
    }

    // 检查 IPv4 数据报的源地址是否是对等方的地址
    if (!listening() && (ip_dgram.header.src != config().destination.ipv4_numeric()))
    {
        return {}; // 如果不在监听状态且源地址不匹配，返回空
    }

    // 检查 IPv4 数据报的协议是否为 TCP
    if (ip_dgram.header.proto != IPv4Header::PROTO_TCP)
    {
        return {}; // 如果不是 TCP 协议，返回空
    }

    // 尝试解析 TCP 段
    TCPSegment tcp_seg;
    if (!parse(tcp_seg, ip_dgram.payload, ip_dgram.header.pseudo_checksum()))
    {
        return {}; // 如果解析失败，返回空
    }

    // 检查 TCP 段的目标端口是否匹配
    if (tcp_seg.udinfo.dst_port != config().source.port())
    {
        return {}; // 如果目标端口不匹配，返回空
    }

    // 如果在监听状态，检查是否需要更新源和目标地址
    if (listening())
    {
        if (tcp_seg.message.sender.SYN && !tcp_seg.message.sender.RST)
        {
            // 更新源和目标地址
            config_mutable().source = Address{inet_ntoa({htobe32(ip_dgram.header.dst)}), config().source.port()};
            config_mutable().destination = Address{inet_ntoa({htobe32(ip_dgram.header.src)}), tcp_seg.udinfo.src_port};
            set_listening(false); // 取消监听状态
        }
        else
        {
            return {}; // 如果没有 SYN 或者有 RST，返回空
        }
    }

    // 检查 TCP 段的源端口是否匹配
    if (tcp_seg.udinfo.src_port != config().destination.port())
    {
        return {}; // 如果源端口不匹配，返回空
    }

    return tcp_seg.message; // 返回有效的 TCP 消息
}

InternetDatagram TCPOverIPv4Adapter::wrap_tcp_in_ip(const TCPMessage &msg)
{
    TCPSegment seg{.message = msg}; // 创建 TCP 段并设置消息
    // 设置 TCP 段的源和目标端口
    seg.udinfo.src_port = config().source.port();
    seg.udinfo.dst_port = config().destination.port();

    // 创建一个 IPv4 数据报并设置其地址和长度
    InternetDatagram ip_dgram;
    ip_dgram.header.src = config().source.ipv4_numeric();                                                         // 设置源地址
    ip_dgram.header.dst = config().destination.ipv4_numeric();                                                    // 设置目标地址
    ip_dgram.header.len = ip_dgram.header.hlen * 4 + 20 /* tcp header len */ + seg.message.sender.payload.size(); // 计算数据报长度

    // 设置有效载荷，计算 TCP 校验和
    seg.compute_checksum(ip_dgram.header.pseudo_checksum()); // 计算 TCP 校验和
    ip_dgram.header.compute_checksum();                      // 计算 IPv4 校验和
    ip_dgram.payload = serialize(seg);                       // 序列化 TCP 段并设置为有效载荷

    return ip_dgram; // 返回封装后的 IPv4 数据报
}
