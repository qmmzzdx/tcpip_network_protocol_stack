#ifndef ARP_MESSAGE_H
#define ARP_MESSAGE_H

#include "parser.h"          // 包含解析器的定义
#include "ipv4_header.h"     // 包含 IPv4 头部的定义
#include "ethernet_header.h" // 包含以太网头部的定义

// 定义 ARP 消息结构体
struct ARPMessage
{
    static constexpr size_t LENGTH = 28;          // ARP 消息的固定长度
    static constexpr uint16_t TYPE_ETHERNET = 1;  // 硬件类型：以太网
    static constexpr uint16_t OPCODE_REQUEST = 1; // 操作码：ARP 请求
    static constexpr uint16_t OPCODE_REPLY = 2;   // 操作码：ARP 回复

    uint16_t hardware_type = TYPE_ETHERNET;                      // 硬件类型，默认为以太网
    uint16_t protocol_type = EthernetHeader::TYPE_IPv4;          // 协议类型，默认为 IPv4
    uint8_t hardware_address_size = sizeof(EthernetHeader::src); // 硬件地址大小
    uint8_t protocol_address_size = sizeof(IPv4Header::src);     // 协议地址大小
    uint16_t opcode{};                                           // 操作码，初始化为 0

    EthernetAddress sender_ethernet_address{}; // 发送者的以太网地址
    uint32_t sender_ip_address{};              // 发送者的 IP 地址

    EthernetAddress target_ethernet_address{}; // 目标的以太网地址
    uint32_t target_ip_address{};              // 目标的 IP 地址

    std::string to_string() const; // 将 ARP 消息转换为字符串表示

    bool supported() const; // 检查 ARP 消息是否被支持

    void parse(Parser &parser);                   // 解析 ARP 消息
    void serialize(Serializer &serializer) const; // 序列化 ARP 消息
};

#endif
