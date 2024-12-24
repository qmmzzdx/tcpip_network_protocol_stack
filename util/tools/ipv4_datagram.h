#ifndef IPV4_DATAGRAM_H
#define IPV4_DATAGRAM_H

#include <memory>
#include <string>
#include <vector>
#include "parser.h"
#include "ipv4_header.h"

struct IPv4Datagram
{
    IPv4Header header{};                // IPv4 头部
    std::vector<std::string> payload{}; // 有效载荷，存储数据部分

    // 解析函数
    void parse(Parser &parser)
    {
        header.parse(parser);          // 解析头部
        parser.all_remaining(payload); // 解析剩余数据作为有效载荷
    }

    // 序列化函数
    void serialize(Serializer &serializer) const
    {
        header.serialize(serializer); // 序列化头部
        for (const auto &x : payload) // 遍历有效载荷
        {
            serializer.buffer(x); // 序列化每个有效载荷
        }
    }
};

// 使用 InternetDatagram 作为 IPv4Datagram 的别名
using InternetDatagram = IPv4Datagram;

#endif
