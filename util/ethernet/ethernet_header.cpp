#include <iomanip>            // 包含用于格式化输出的库
#include <sstream>            // 包含字符串流的库
#include "ethernet_header.h"  // 包含以太网头部的定义

// 将以太网地址转换为字符串表示
std::string to_string(const EthernetAddress address)
{
    std::stringstream ss{};                                 // 创建一个字符串流对象
    for (size_t index = 0; index < address.size(); index++) // 遍历以太网地址的每个字节
    {
        ss.width(2);                               // 设置输出宽度为 2
        ss << std::setfill('0')                    // 用 '0' 填充空白
           << std::hex                             // 设置输出为十六进制
           << static_cast<int>(address.at(index)); // 将地址的每个字节转换为整数并输出
        if (index != address.size() - 1)           // 如果不是最后一个字节
        {
            ss << ":"; // 在字节之间添加冒号分隔符
        }
    }
    return ss.str(); // 返回格式化后的字符串
}

// 将以太网头部的内容转换为字符串表示
std::string EthernetHeader::to_string() const
{
    std::stringstream ss{};            // 创建一个字符串流对象
    ss << "dst=" << ::to_string(dst);  // 将目标地址转换为字符串并添加到输出中
    ss << " src=" << ::to_string(src); // 将源地址转换为字符串并添加到输出中
    ss << " type=";                    // 添加类型前缀
    switch (type)                      // 根据帧类型输出相应的字符串
    {
    case TYPE_IPv4:
        ss << "IPv4"; // 如果类型是 IPv4，添加相应的字符串
        break;
    case TYPE_ARP:
        ss << "ARP"; // 如果类型是 ARP，添加相应的字符串
        break;
    default:
        ss << "[unknown type " << std::hex << type << "!]"; // 对于未知类型，输出十六进制类型值
        break;
    }

    return ss.str(); // 返回格式化后的字符串
}

// 解析以太网头部
void EthernetHeader::parse(Parser &parser)
{
    // 读取目标地址
    for (auto &b : dst) // 遍历目标地址的每个字节
    {
        parser.integer(b); // 使用解析器读取每个字节
    }

    // 读取源地址
    for (auto &b : src) // 遍历源地址的每个字节
    {
        parser.integer(b); // 使用解析器读取每个字节
    }

    // 读取帧类型（例如 IPv4、ARP 或其他类型）
    parser.integer(type); // 使用解析器读取帧类型
}

// 序列化以太网头部
void EthernetHeader::serialize(Serializer &serializer) const
{
    // 写入目标地址
    for (const auto &b : dst) // 遍历目标地址的每个字节
    {
        serializer.integer(b); // 使用序列化器写入每个字节
    }

    // 写入源地址
    for (const auto &b : src) // 遍历源地址的每个字节
    {
        serializer.integer(b); // 使用序列化器写入每个字节
    }

    // 写入帧类型（例如 IPv4、ARP 或其他类型）
    serializer.integer(type); // 使用序列化器写入帧类型
}
