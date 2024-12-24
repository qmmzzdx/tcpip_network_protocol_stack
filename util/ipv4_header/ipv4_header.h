#ifndef IPV4_HEADER_H
#define IPV4_HEADER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include "parser.h" // 引入自定义的解析器头文件

// IPv4 互联网数据报头（注意：不支持 IP 选项）
struct IPv4Header
{
    // 常量定义
    static constexpr size_t LENGTH = 20;        // IPv4 头部长度，不包括选项
    static constexpr uint8_t DEFAULT_TTL = 128; // 默认的生存时间（TTL）值
    static constexpr uint8_t PROTO_TCP = 6;     // TCP 协议的协议号

    // 返回序列化后的长度
    static constexpr uint64_t serialized_length() { return LENGTH; }

    /*
     *   0                   1                   2                   3
     *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |Version|  IHL  |Type of Service|          Total Length         |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |         Identification        |Flags|      Fragment Offset    |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |  Time to Live |    Protocol   |         Header Checksum       |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |                       Source Address                          |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |                    Destination Address                        |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |                    Options                    |    Padding    |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */

    // IPv4 头部字段
    uint8_t ver = 4;           // IP 版本，IPv4 的值为 4
    uint8_t hlen = LENGTH / 4; // 头部长度，以 32 位为单位（5 表示 20 字节）
    uint8_t tos = 0;           // 服务类型（Type of Service）
    uint16_t len = 0;          // 数据包的总长度（包括头部和数据）
    uint16_t id = 0;           // 标识符，用于唯一标识数据包
    bool df = true;            // 不分片标志（Don't Fragment）
    bool mf = false;           // 更多分片标志（More Fragments）
    uint16_t offset = 0;       // 分片偏移字段
    uint8_t ttl = DEFAULT_TTL; // 生存时间（TTL），用于限制数据包的生命周期
    uint8_t proto = PROTO_TCP; // 协议字段，指示上层协议（如 TCP）
    uint16_t cksum = 0;        // 校验和字段，用于错误检测
    uint32_t src = 0;          // 源地址
    uint32_t dst = 0;          // 目的地址

    // 计算有效载荷的长度
    uint16_t payload_length() const;

    // 计算伪头部对 TCP 校验和的贡献
    uint32_t pseudo_checksum() const;

    // 计算并设置校验和的正确值
    void compute_checksum();

    // 返回一个包含人类可读格式的头部字符串
    std::string to_string() const;

    // 解析头部数据
    void parse(Parser &parser);

    // 序列化头部数据
    void serialize(Serializer &serializer) const;
};

#endif
