#include <iomanip>        // 包含用于格式化输出的库
#include <sstream>        // 包含字符串流的库
#include <arpa/inet.h>    // 包含网络地址转换的库
#include "arp_message.h"  // 包含 ARP 消息的定义

// 检查 ARP 消息是否被支持
bool ARPMessage::supported() const
{
    // 硬件类型必须是以太网，协议类型必须是 IPv4，硬件地址和协议地址大小必须匹配
    // 操作码必须是请求或回复
    return hardware_type == TYPE_ETHERNET && protocol_type == EthernetHeader::TYPE_IPv4 && hardware_address_size == sizeof(EthernetHeader::src) && protocol_address_size == sizeof(IPv4Header::src) && (opcode == OPCODE_REQUEST || opcode == OPCODE_REPLY);
}

// 将 ARP 消息转换为字符串表示
std::string ARPMessage::to_string() const
{
    std::stringstream ss{};                    // 创建一个字符串流对象
    std::string opcode_str = "(unknown type)"; // 默认操作码字符串

    // 根据操作码设置相应的字符串
    if (opcode == OPCODE_REQUEST)
    {
        opcode_str = "REQUEST"; // 如果操作码是请求
    }
    if (opcode == OPCODE_REPLY)
    {
        opcode_str = "REPLY"; // 如果操作码是回复
    }

    // 构建字符串表示，包括操作码、发送者和目标的以太网地址和 IP 地址
    ss << "opcode=" << opcode_str
       << ", sender=" << ::to_string(sender_ethernet_address) << "/"
       << inet_ntoa({htobe32(sender_ip_address)}) << ", target="
       << ::to_string(target_ethernet_address) << "/"
       << inet_ntoa({htobe32(target_ip_address)});

    return ss.str(); // 返回格式化后的字符串
}

// 解析 ARP 消息
void ARPMessage::parse(Parser &parser)
{
    // 读取 ARP 消息的各个字段
    parser.integer(hardware_type);
    parser.integer(protocol_type);
    parser.integer(hardware_address_size);
    parser.integer(protocol_address_size);
    parser.integer(opcode);

    // 检查 ARP 消息是否被支持
    if (!supported())
    {
        parser.set_error(); // 如果不支持，设置解析器错误状态
        return;             // 退出解析
    }

    // 读取发送者的以太网地址和 IP 地址
    for (auto &b : sender_ethernet_address)
    {
        parser.integer(b); // 逐字节读取以太网地址
    }
    parser.integer(sender_ip_address); // 读取发送者的 IP 地址

    // 读取目标的以太网地址和 IP 地址
    for (auto &b : target_ethernet_address)
    {
        parser.integer(b); // 逐字节读取以太网地址
    }
    parser.integer(target_ip_address); // 读取目标的 IP 地址
}

// 序列化 ARP 消息
void ARPMessage::serialize(Serializer &serializer) const
{
    // 检查 ARP 消息是否被支持
    if (!supported())
    {
        throw std::runtime_error("ARPMessage: unsupported field combination (must be Ethernet/IP, and request or reply)");
    }

    // 写入 ARP 消息的各个字段
    serializer.integer(hardware_type);
    serializer.integer(protocol_type);
    serializer.integer(hardware_address_size);
    serializer.integer(protocol_address_size);
    serializer.integer(opcode);

    // 写入发送者的以太网地址和 IP 地址
    for (const auto &b : sender_ethernet_address)
    {
        serializer.integer(b); // 逐字节写入以太网地址
    }
    serializer.integer(sender_ip_address); // 写入发送者的 IP 地址

    // 写入目标的以太网地址和 IP 地址
    for (const auto &b : target_ethernet_address)
    {
        serializer.integer(b); // 逐字节写入以太网地址
    }
    serializer.integer(target_ip_address); // 写入目标的 IP 地址
}
