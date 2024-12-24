#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <cstdint> // 包含固定宽度整数类型
#include <string>  // 包含字符串类
#include <vector>  // 包含向量类

// InternetChecksum 类用于计算互联网协议的校验和
class InternetChecksum
{
private:
    uint32_t sum_; // 存储当前的校验和
    bool parity_;  // 用于跟踪数据的奇偶性（高位或低位）

public:
    // 构造函数，初始化校验和，默认为 0
    explicit InternetChecksum(uint32_t sum = 0) : sum_(sum), parity_(false) {}

    // 将字符串数据添加到校验和中
    void add(std::string_view data)
    {
        for (const uint8_t byte : data) // 遍历字符串中的每个字节
        {
            uint16_t val = (parity_ ? byte : byte << 8); // 将字节转换为 16 位整数，并根据奇偶性决定高低位
            sum_ += val;                                 // 将值添加到校验和中
            parity_ = !parity_;                          // 切换奇偶性
        }
    }

    // 计算并返回当前校验和的值
    uint16_t value() const
    {
        uint32_t ret = sum_; // 复制当前校验和

        // 如果校验和大于 16 位，进行折叠
        while (ret > 0xffff)
        {
            ret = (ret >> 16) + (ret & 0xffff); // 将高 16 位与低 16 位相加
        }
        return ~static_cast<uint16_t>(ret); // 返回校验和的反码
    }

    // 将字符串向量中的每个字符串添加到校验和中
    void add(const std::vector<std::string> &data)
    {
        for (const auto &str : data) // 遍历向量中的每个字符串
        {
            add(str); // 调用 add 函数处理每个字符串
        }
    }

    // 将字符串视图向量中的每个字符串视图添加到校验和中
    void add(const std::vector<std::string_view> &data)
    {
        for (const auto &str_view : data) // 遍历向量中的每个字符串视图
        {
            add(str_view); // 调用 add 函数处理每个字符串视图
        }
    }
};

#endif // CHECKSUM_H
