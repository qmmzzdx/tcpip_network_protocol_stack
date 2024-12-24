#ifndef WRAPPING_INTEGERS_H
#define WRAPPING_INTEGERS_H

#include <cstdint>

/*
 * Wrap32 类表示一个 32 位无符号整数，该整数：
 *    - 从一个任意的“零点”（初始值）开始，并且
 *    - 当达到 2^32 - 1 时回绕到零。
 */

class Wrap32
{
public:
    // 构造函数：使用一个原始的 32 位无符号整数值初始化 Wrap32 对象。
    explicit Wrap32(uint32_t raw_value) : raw_value_(raw_value) {}

    /*
     * 静态方法 wrap：给定一个绝对序列号 n 和一个零点，构造一个 Wrap32 对象。
     * 该方法用于将一个 64 位的绝对序列号 n 转换为相对于零点的 Wrap32 值。
     */
    static Wrap32 wrap(uint64_t n, Wrap32 zero_point);

    /*
     * unwrap 方法：返回一个绝对序列号，该序列号在给定的零点和“检查点”下会映射到当前的 Wrap32。
     *
     * 由于多个绝对序列号可能映射到同一个 Wrap32 值，该方法返回最接近检查点的那个绝对序列号。
     * 这在处理序列号时非常有用，尤其是在网络协议中需要处理序列号回绕的情况。
     */
    uint64_t unwrap(Wrap32 zero_point, uint64_t checkpoint) const;

    // 操作符重载：定义 Wrap32 对象与一个 32 位无符号整数相加的操作。
    Wrap32 operator+(uint32_t n) const { return Wrap32{raw_value_ + n}; }

    // 操作符重载：定义两个 Wrap32 对象的相等比较。
    bool operator==(const Wrap32 &other) const { return raw_value_ == other.raw_value_; }

protected:
    // 存储 Wrap32 的原始 32 位无符号整数值。
    uint32_t raw_value_{};
};

#endif
