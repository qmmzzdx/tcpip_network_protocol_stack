#include <cstddef>
#include <sstream>
#include <array>
#include <arpa/inet.h>
#include "checksum.h"
#include "ipv4_header.h"

// 解析字符串
void IPv4Header::parse(Parser &parser)
{
    uint8_t first_byte{};
    parser.integer(first_byte);
    ver = first_byte >> 4;    // 版本
    hlen = first_byte & 0x0f; // 头部长度
    parser.integer(tos);      // 服务类型
    parser.integer(len);      // 总长度
    parser.integer(id);       // 标识符

    uint16_t fo_val{};
    parser.integer(fo_val);
    df = static_cast<bool>(fo_val & 0x4000); // 不分片标志
    mf = static_cast<bool>(fo_val & 0x2000); // 更多分片标志
    offset = fo_val & 0x1fff;                // 分片偏移

    parser.integer(ttl);   // 生存时间
    parser.integer(proto); // 协议
    parser.integer(cksum); // 校验和
    parser.integer(src);   // 源地址
    parser.integer(dst);   // 目的地址

    // 检查版本是否为 IPv4
    if (ver != 4)
    {
        parser.set_error();
    }

    // 检查头部长度是否有效
    if (hlen < 5)
    {
        parser.set_error();
    }

    // 如果解析过程中有错误，直接返回
    if (parser.has_error())
    {
        return;
    }

    // 移除多余的前缀
    parser.remove_prefix(static_cast<uint64_t>(hlen) * 4 - IPv4Header::LENGTH);

    // 验证校验和
    const uint16_t given_cksum = cksum;
    compute_checksum(); // 计算校验和
    if (cksum != given_cksum)
    {
        parser.set_error(); // 如果校验和不匹配，设置错误
    }
}

// 序列化 IPv4Header（不重新计算校验和）
void IPv4Header::serialize(Serializer &serializer) const
{
    // 一致性检查
    if (ver != 4)
    {
        throw std::runtime_error("wrong IP version"); // 抛出异常
    }

    // 组合版本和头部长度
    const uint8_t first_byte = (static_cast<uint32_t>(ver) << 4) | (hlen & 0xfU);
    serializer.integer(first_byte); // 版本和头部长度
    serializer.integer(tos);        // 服务类型
    serializer.integer(len);        // 总长度
    serializer.integer(id);         // 标识符

    // 组合分片标志和偏移
    const uint16_t fo_val = (df ? 0x4000U : 0) | (mf ? 0x2000U : 0) | (offset & 0x1fffU);
    serializer.integer(fo_val); // 分片标志和偏移

    serializer.integer(ttl);   // 生存时间
    serializer.integer(proto); // 协议
    serializer.integer(cksum); // 校验和
    serializer.integer(src);   // 源地址
    serializer.integer(dst);   // 目的地址
}

// 计算有效载荷的长度
uint16_t IPv4Header::payload_length() const
{
    return len - 4 * hlen; // 总长度减去头部长度
}

// 计算伪头部的校验和
// 0      7 8     15 16    23 24    31
// +--------+--------+--------+--------+
// |          source address           |
// +--------+--------+--------+--------+
// |        destination address        |
// +--------+--------+--------+--------+
// |  zero  |protocol|  payload length |
// +--------+--------+--------+--------+
uint32_t IPv4Header::pseudo_checksum() const
{
    uint32_t pcksum = (src >> 16) + static_cast<uint16_t>(src); // 源地址
    pcksum += (dst >> 16) + static_cast<uint16_t>(dst);         // 目的地址
    pcksum += proto;                                            // 协议
    pcksum += payload_length();                                 // 有效载荷长度
    return pcksum;                                              // 返回伪校验和
}

// 计算校验和
void IPv4Header::compute_checksum()
{
    cksum = 0;    // 重置校验和
    Serializer s; // 创建序列化器
    serialize(s); // 序列化头部

    // 计算校验和 -- 仅针对头部
    InternetChecksum check; // 创建校验和计算器
    check.add(s.output());  // 添加序列化的输出
    cksum = check.value();  // 获取计算出的校验和
}

// 返回一个包含人类可读格式的头部字符串
std::string IPv4Header::to_string() const
{
    std::stringstream ss{}; // 创建字符串流
    ss << std::hex << std::boolalpha << "IPv" << +ver << " len=" << std::dec << +len
       << " protocol=" << +proto << " ttl=" << std::to_string(ttl)
       << " src=" << inet_ntoa({htobe32(src)})  // 转换源地址为字符串
       << " dst=" << inet_ntoa({htobe32(dst)}); // 转换目的地址为字符串
    return ss.str();                            // 返回字符串
}
