#include "tuntap_adapter.h" // 包含 TUN/TAP 适配器的定义
#include "parser.h"         // 包含解析器的定义

// 从 TUN 设备读取 TCP 消息
std::optional<TCPMessage> TCPOverIPv4OverTunFdAdapter::read()
{
    std::vector<std::string> strs(2);        // 创建一个包含两个字符串的向量
    strs.front().resize(IPv4Header::LENGTH); // 将第一个字符串的大小调整为 IPv4 头部的长度
    _tun.read(strs);                         // 从 TUN 设备读取数据到 strs 向量中

    InternetDatagram ip_dgram;                                         // 创建一个 InternetDatagram 对象，用于存储解析后的 IP 数据报
    const std::vector<std::string> buffers = {strs.at(0), strs.at(1)}; // 创建一个包含两个缓冲区的向量

    // 尝试解析 IP 数据报
    if (parse(ip_dgram, buffers))
    {
        // 如果解析成功，解包 TCP 消息并返回
        return unwrap_tcp_in_ip(ip_dgram);
    }
    return {}; // 如果解析失败，返回空的 optional
}

// 显式实例化 LossyFdAdapter<TCPOverIPv4OverTunFdAdapter> 模板
template class LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>;
