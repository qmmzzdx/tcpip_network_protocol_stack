#ifndef TCP_OVER_IP_H
#define TCP_OVER_IP_H

#include <optional>         // 包含 std::optional 的定义
#include "fd_adapter.h"     // 包含文件描述符适配器的基类定义
#include "ipv4_datagram.h"  // 包含 IPv4 数据报的定义
#include "tcp_segment.h"    // 包含 TCP 段的定义

// TCPOverIPv4Adapter 类用于将 TCP 段转换为序列化的 IPv4 数据报
class TCPOverIPv4Adapter : public FdAdapterBase
{
public:
    // 从 IPv4 数据报中解包 TCP 消息
    std::optional<TCPMessage> unwrap_tcp_in_ip(const InternetDatagram &ip_dgram);

    // 将 TCP 消息封装到 IPv4 数据报中
    InternetDatagram wrap_tcp_in_ip(const TCPMessage &msg);
};

#endif
