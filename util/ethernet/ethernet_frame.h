#ifndef ETHERNET_FRAME_H
#define ETHERNET_FRAME_H

#include <vector>             // 包含动态数组的库
#include "parser.h"           // 包含解析器的定义
#include "ethernet_header.h"  // 包含以太网头部的定义

// 定义以太网帧结构体
struct EthernetFrame
{
    EthernetHeader header{};            // 以太网头部，使用默认构造函数初始化
    std::vector<std::string> payload{}; // 负载，使用动态数组存储字符串

    // 解析以太网帧
    void parse(Parser &parser)
    {
        header.parse(parser);          // 解析以太网头部
        parser.all_remaining(payload); // 解析剩余数据并存储到负载中
    }

    // 序列化以太网帧
    void serialize(Serializer &serializer) const
    {
        header.serialize(serializer); // 序列化以太网头部
        serializer.buffer(payload);   // 序列化负载数据
    }
};

#endif
