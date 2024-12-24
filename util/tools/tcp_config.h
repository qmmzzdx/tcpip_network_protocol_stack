#ifndef TCP_CONFIG_H
#define TCP_CONFIG_H

#include <cstddef>             // 包含 size_t 的定义
#include <cstdint>             // 包含固定宽度整数类型的定义
#include <optional>            // 包含 std::optional 的定义
#include "address.h"           // 包含自定义的地址类定义
#include "wrapping_integers.h" // 包含自定义的包装整数类定义

// TCPConfig 类用于配置 TCP 发送器和接收器的参数
class TCPConfig
{
public:
    // 默认接收和发送缓冲区的容量，单位为字节
    static constexpr size_t DEFAULT_CAPACITY = 64000;
    // 最大有效载荷大小，单位为字节，适用于实际互联网的保守估计
    static constexpr size_t MAX_PAYLOAD_SIZE = 1000;
    // 默认重传超时，单位为毫秒（1 秒）
    static constexpr uint16_t TIMEOUT_DFLT = 1000;
    // 最大重传尝试次数，超过此次数将放弃重传
    static constexpr unsigned MAX_RETX_ATTEMPTS = 8;

    uint16_t rt_timeout = TIMEOUT_DFLT;      // 初始重传超时值，单位为毫秒
    size_t recv_capacity = DEFAULT_CAPACITY; // 接收缓冲区的容量，单位为字节
    size_t send_capacity = DEFAULT_CAPACITY; // 发送缓冲区的容量，单位为字节
    Wrap32 isn{137};                         // 默认初始序列号，使用 Wrap32 类型
};

// FdAdapterConfig 类用于配置与文件描述符适配器相关的参数
class FdAdapterConfig
{
public:
    // 源地址和端口，使用 Address 类型，默认值为 IP 地址 "0" 和端口 0
    Address source{"0", 0};
    // 目标地址和端口，使用 Address 类型，默认值为 IP 地址 "0" 和端口 0
    Address destination{"0", 0};

    // 下行丢包率，单位为百分比，默认为 0（表示没有丢包）
    uint16_t loss_rate_dn = 0;
    // 上行丢包率，单位为百分比，默认为 0（表示没有丢包）
    uint16_t loss_rate_up = 0;
};

#endif
