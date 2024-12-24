#ifndef UDINFO_H
#define UDINFO_H

#include <cstdint>

// 定义一个结构体，用于表示用户数据报（UDP）信息
struct UserDatagramInfo
{
    uint16_t src_port; // 源端口号，表示发送方的端口
    uint16_t dst_port; // 目标端口号，表示接收方的端口
    uint16_t cksum;    // 校验和，用于验证数据在传输过程中的完整性
};

#endif
