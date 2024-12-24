#ifndef ETHERNET_HEADER_H
#define ETHERNET_HEADER_H

#include <array>     // 包含 std::array 的定义，用于固定大小的数组
#include <cstdint>   // 包含标准整数类型的定义，如 uint8_t 和 uint16_t
#include <string>    // 包含 std::string 的定义，用于字符串处理
#include "parser.h"  // 包含解析器的定义

// 定义以太网地址类型，使用一个包含 6 个 uint8_t 的数组
using EthernetAddress = std::array<uint8_t, 6>;

// 定义以太网广播地址，所有位都为 1
constexpr EthernetAddress ETHERNET_BROADCAST = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// 将以太网地址转换为字符串的函数声明
std::string to_string(EthernetAddress address);

// 定义以太网头部结构
struct EthernetHeader
{
    // 以太网头部的长度，单位为字节
    static constexpr size_t LENGTH = 14;

    // 以太网帧类型字段的常量，表示 IPv4
    static constexpr uint16_t TYPE_IPv4 = 0x800;

    // 以太网帧类型字段的常量，表示 ARP
    static constexpr uint16_t TYPE_ARP = 0x806;

    // 目标以太网地址
    EthernetAddress dst;

    // 源以太网地址
    EthernetAddress src;

    // 以太网帧类型
    uint16_t type;

    // 将以太网头部转换为字符串的成员函数
    std::string to_string() const;

    // 解析以太网头部的成员函数
    void parse(Parser &parser);

    // 序列化以太网头部的成员函数
    void serialize(Serializer &serializer) const;
};

#endif
