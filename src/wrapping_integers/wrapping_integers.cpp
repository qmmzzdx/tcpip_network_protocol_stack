#include "wrapping_integers.h"

using namespace std;

// Wrap32 类的实现

// wrap 方法：将一个 64 位无符号整数 n 映射到一个 32 位的 Wrap32 对象
// zero_point 是映射的起始点
Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point)
{
    // 将 64 位整数 n 转换为 32 位整数，并加上 zero_point 的值
    // 由于是 32 位整数，任何溢出都会自动处理（即模 2^32）
    return zero_point + static_cast<uint32_t>(n);
}

// unwrap 方法：将当前 Wrap32 对象解映射回一个 64 位无符号整数
// zero_point 是映射的起始点，checkpoint 是一个参考点，用于确定解映射的范围
uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const
{
    // 计算当前对象的 raw_value_ 与从 checkpoint 开始的 wrap 值之间的偏移量
    // 这里的偏移量可能是负数，表示当前值在 checkpoint 之前
    int32_t offset = raw_value_ - wrap(checkpoint, zero_point).raw_value_;

    // 计算绝对序列号
    // 将偏移量加到 checkpoint 上，得到一个可能的绝对序列号
    int64_t absseq = checkpoint + offset;

    // 如果计算出的绝对序列号为负数，则加上 2^32 以确保返回值为正
    // 这一步是为了处理可能的负偏移导致的负数结果
    return absseq < 0 ? absseq + (1ULL << 32) : absseq;
}
