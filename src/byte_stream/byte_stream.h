#ifndef BYTE_STREAM_H
#define BYTE_STREAM_H

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <stdexcept>

// 前向声明 Reader 和 Writer 类
class Reader;
class Writer;

// ByteStream 类是一个字节流的基础类，提供了流的基本功能
class ByteStream
{
public:
    // 构造函数，初始化流的容量
    explicit ByteStream(uint64_t capacity);

    // 获取 Reader 和 Writer 的引用
    Reader &reader();
    const Reader &reader() const;
    Writer &writer();
    const Writer &writer() const;

    // 设置和检查错误状态
    void set_error() { error_ = true; };
    bool has_error() const { return error_; };

protected:
    uint64_t capacity_;                      // 流的最大容量
    bool error_{false};                      // 错误状态标志
    bool closed_{false};                     // 流是否已关闭
    uint64_t bytes_pushed_{0};               // 已推入的字节数
    uint64_t bytes_popped_{0};               // 已弹出的字节数
    uint64_t bytes_buffered_{0};             // 当前缓冲的字节数
    uint64_t remove_prefix_len_{0};          // 用于管理缓冲区前缀的长度
    std::deque<std::string> bytes_stream_{}; // 用于存储字节流的数据结构
};

// Writer 类继承自 ByteStream，提供写入功能
class Writer : public ByteStream
{
public:
    // 将数据推入流中
    void push(std::string data);
    // 关闭流
    void close();

    // 检查流是否已关闭
    bool is_closed() const;
    // 获取流的可用容量
    uint64_t available_capacity() const;
    // 获取已推入的字节数
    uint64_t bytes_pushed() const;
};

// Reader 类继承自 ByteStream，提供读取功能
class Reader : public ByteStream
{
public:
    // 查看流中当前可读取的数据
    std::string_view peek() const;
    // 从流中弹出指定长度的数据
    void pop(uint64_t len);

    // 检查流是否已完成（即关闭且无数据可读）
    bool is_finished() const;
    // 获取当前缓冲的字节数
    uint64_t bytes_buffered() const;
    // 获取已弹出的字节数
    uint64_t bytes_popped() const;
};

/*
 * read: 一个辅助函数，从 ByteStream Reader 中读取最多 `len` 字节的数据到一个字符串中。
 */
void read(Reader &reader, uint64_t len, std::string &out);

#endif
