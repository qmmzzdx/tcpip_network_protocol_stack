#ifndef REASSEMBLER_H
#define REASSEMBLER_H

#include <iostream>
#include <list>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include "byte_stream.h"

// Reassembler 类用于将分段的字节流重新组装成完整的字节流。
class Reassembler
{
public:
    // 构造函数，接受一个 ByteStream 对象用于输出。
    explicit Reassembler(ByteStream &&output) : output_(std::move(output)) {}

    /*
     * 插入一个新的子字符串以重新组装成 ByteStream。
     *   `first_index`: 子字符串的第一个字节的索引
     *   `data`: 子字符串本身
     *   `is_last_substring`: 该子字符串是否表示流的结束
     *   `output`: 一个可变引用，指向 Writer
     *
     * Reassembler 的任务是将索引的子字符串（可能是无序的和重叠的）重新组装回原始的 ByteStream。
     * 一旦 Reassembler 知道流中的下一个字节，它就应该将其写入输出。
     *
     * 如果 Reassembler 知道适合流的可用容量的字节，但由于早期字节未知而无法写入，
     * 它应该在内部存储这些字节，直到填补空白。
     *
     * Reassembler 应该丢弃超出流的可用容量的字节（即使早期的空白被填补也无法写入的字节）。
     *
     * 在写入最后一个字节后，Reassembler 应该关闭流。
     */
    void insert(uint64_t first_index, std::string data, bool is_last_substring);

    // 返回存储在 Reassembler 中的字节数。
    uint64_t bytes_pending() const;

    // 访问输出流的读取器
    Reader &reader() { return output_.reader(); }
    const Reader &reader() const { return output_.reader(); }

    // 访问输出流的写入器，但只能是 const 访问（不能从外部写入）
    const Writer &writer() const { return output_.writer(); }

private:
    ByteStream output_; // Reassembler 写入这个 ByteStream
    struct Interval
    {
        uint64_t beg_idx;         // 区间的起始索引
        uint64_t end_idx;         // 区间的结束索引
        std::string interval_str; // 区间内的字符串
        // 定义区间的比较运算符，用于排序
        bool operator<(const Interval &other) const
        {
            return beg_idx != other.beg_idx ? beg_idx < other.beg_idx : end_idx < other.end_idx;
        };
    };
    std::list<Interval> buffers_{};       // 用于存储未组装的字节区间
    uint64_t first_unassembled_index_{0}; // 第一个未组装的字节的索引
    uint64_t eof_index_{UINT64_MAX};      // 流结束的索引，初始为最大值
};

#endif
