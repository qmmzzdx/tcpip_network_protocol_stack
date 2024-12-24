#ifndef PARSER_H
#define PARSER_H

#include <cstdint>     // 包含固定宽度整数类型
#include <cstring>     // 包含 C 字符串处理函数
#include <deque>       // 包含双端队列
#include <span>        // 包含 std::span
#include <numeric>     // 包含数值算法
#include <concepts>    // 包含概念
#include <stdexcept>   // 包含标准异常类
#include <string>      // 包含字符串类
#include <vector>      // 包含向量类
#include <string_view> // 包含字符串视图
#include <algorithm>   // 包含算法函数

// Parser 类用于解析输入数据
class Parser
{
    // BufferList 类用于管理输入缓冲区
    class BufferList
    {
        uint64_t size_{};                  // 当前缓冲区的总大小
        std::deque<std::string> buffer_{}; // 存储字符串的双端队列
        uint64_t skip_{};                  // 跳过的字节数

    public:
        // 构造函数，接受字符串向量并将其添加到缓冲区
        explicit BufferList(const std::vector<std::string> &buffers)
        {
            for (const auto &x : buffers)
            {
                append(x); // 将每个字符串添加到缓冲区
            }
        }

        // 获取当前缓冲区的大小
        uint64_t size() const { return size_; }
        // 获取序列化长度（与大小相同）
        uint64_t serialized_length() const { return size(); }
        // 检查缓冲区是否为空
        bool empty() const { return size_ == 0; }

        // 查看缓冲区的前缀
        std::string_view peek() const
        {
            if (buffer_.empty())
            {
                throw std::runtime_error("peek on empty BufferList"); // 如果缓冲区为空，抛出异常
            }
            return std::string_view{buffer_.front()}.substr(skip_); // 返回前缀的字符串视图
        }

        // 移除前缀
        void remove_prefix(uint64_t len)
        {
            while (len > 0 && !buffer_.empty()) // 当 len 不为 0 且缓冲区不为空时
            {
                const uint64_t to_pop_now = std::min(len, peek().size()); // 计算要移除的字节数
                skip_ += to_pop_now;                                      // 更新跳过的字节数
                len -= to_pop_now;                                        // 减少要移除的字节数
                size_ -= to_pop_now;                                      // 更新缓冲区大小
                if (skip_ == buffer_.front().size())                      // 如果跳过的字节数等于当前字符串的大小
                {
                    buffer_.pop_front(); // 移除当前字符串
                    skip_ = 0;           // 重置跳过的字节数
                }
            }
        }

        // 将所有缓冲区内容转储到字符串向量中
        void dump_all(std::vector<std::string> &out)
        {
            out.clear(); // 清空输出向量
            if (empty()) // 如果缓冲区为空，直接返回
            {
                return;
            }
            std::string first_str = std::move(buffer_.front()); // 移动第一个字符串
            if (skip_ > 0)                                      // 如果有跳过的字节
            {
                first_str = first_str.substr(skip_); // 调整字符串
            }
            out.emplace_back(std::move(first_str)); // 将第一个字符串添加到输出向量
            buffer_.pop_front();                    // 移除第一个字符串
            for (auto &&x : buffer_)                // 遍历剩余的字符串
            {
                out.emplace_back(std::move(x)); // 将其添加到输出向量
            }
        }

        // 将所有缓冲区内容转储到单个字符串中
        void dump_all(std::string &out)
        {
            std::vector<std::string> concat; // 创建临时向量
            dump_all(concat);                // 调用 dump_all 将内容转储到临时向量
            if (concat.size() == 1)          // 如果只有一个字符串
            {
                out = std::move(concat.front()); // 直接移动到输出字符串
                return;
            }

            out.clear();                 // 清空输出字符串
            for (const auto &s : concat) // 遍历临时向量
            {
                out.append(s); // 将每个字符串添加到输出字符串
            }
        }

        // 获取当前缓冲区的字符串视图
        std::vector<std::string_view> buffer() const
        {
            if (empty()) // 如果缓冲区为空，返回空向量
            {
                return {};
            }
            std::vector<std::string_view> ret; // 创建字符串视图向量
            ret.reserve(buffer_.size());       // 预留空间
            auto tmp_skip = skip_;             // 临时保存跳过的字节数
            for (const auto &x : buffer_)      // 遍历缓冲区
            {
                ret.push_back(std::string_view{x}.substr(tmp_skip)); // 添加字符串视图
                tmp_skip = 0;                                        // 重置跳过的字节数
            }
            return ret; // 返回字符串视图向量
        }

        // 将字符串添加到缓冲区
        void append(std::string str)
        {
            size_ += str.size();               // 更新缓冲区大小
            buffer_.push_back(std::move(str)); // 将字符串移动到缓冲区
        }
    };

    BufferList input_; // 输入缓冲区
    bool error_{};     // 错误标志

    // 检查请求的大小是否超过当前缓冲区大小
    void check_size(const size_t size)
    {
        if (size > input_.size()) // 如果请求的大小超过缓冲区大小
        {
            error_ = true; // 设置错误标志
        }
    }

public:
    // 构造函数，接受字符串向量作为输入
    explicit Parser(const std::vector<std::string> &input) : input_(input) {}

    // 获取输入缓冲区的引用
    const BufferList &input() const { return input_; }

    // 检查是否有错误
    bool has_error() const { return error_; }
    // 设置错误标志
    void set_error() { error_ = true; }
    // 移除前缀
    void remove_prefix(size_t n) { input_.remove_prefix(n); }

    // 解析无符号整数
    template <std::unsigned_integral T>
    void integer(T &out)
    {
        check_size(sizeof(T)); // 检查大小
        if (has_error())       // 如果有错误，直接返回
        {
            return;
        }

        if constexpr (sizeof(T) == 1) // 如果是 1 字节
        {
            out = static_cast<uint8_t>(input_.peek().front()); // 直接读取
            input_.remove_prefix(1);                           // 移除前缀
            return;
        }
        else // 对于多字节整数
        {
            out = static_cast<T>(0); // 初始化为 0
            for (size_t i = 0; i < sizeof(T); i++)
            {
                out <<= 8;                                          // 左移 8 位
                out |= static_cast<uint8_t>(input_.peek().front()); // 读取下一个字节
                input_.remove_prefix(1);                            // 移除前缀
            }
        }
    }

    // 解析字符串到给定的字符数组
    void string(std::span<char> out)
    {
        check_size(out.size()); // 检查大小
        if (has_error())        // 如果有错误，直接返回
        {
            return;
        }

        auto next = out.begin();  // 指向输出的开始
        while (next != out.end()) // 当未到达输出末尾
        {
            const auto view = input_.peek().substr(0, out.end() - next); // 获取可用的字符串视图
            next = std::copy(view.begin(), view.end(), next);            // 复制到输出
            input_.remove_prefix(view.size());                           // 移除前缀
        }
    }

    // 获取所有剩余的输入并转储到字符串向量
    void all_remaining(std::vector<std::string> &out) { input_.dump_all(out); }
    // 获取所有剩余的输入并转储到单个字符串
    void all_remaining(std::string &out) { input_.dump_all(out); }
    // 获取当前缓冲区的字符串视图
    std::vector<std::string_view> buffer() const { return input_.buffer(); }
};

// Serializer 类用于序列化数据
class Serializer
{
    std::vector<std::string> output_{}; // 输出字符串向量
    std::string buffer_{};              // 临时缓冲区

public:
    Serializer() = default;                                                   // 默认构造函数
    explicit Serializer(std::string &&buffer) : buffer_(std::move(buffer)) {} // 移动构造函数

    // 序列化无符号整数
    template <std::unsigned_integral T>
    void integer(const T val)
    {
        constexpr uint64_t len = sizeof(T); // 获取类型大小

        for (uint64_t i = 0; i < len; ++i)
        {
            const uint8_t byte_val = val >> ((len - i - 1) * 8); // 获取每个字节
            buffer_.push_back(byte_val);                         // 添加到缓冲区
        }
    }

    // 将字符串缓冲区添加到输出
    void buffer(std::string buf)
    {
        flush();          // 刷新当前缓冲区
        if (!buf.empty()) // 如果缓冲区不为空
        {
            output_.push_back(std::move(buf)); // 添加到输出
        }
    }

    // 将字符串向量添加到输出
    void buffer(const std::vector<std::string> &bufs)
    {
        for (const auto &b : bufs) // 遍历字符串向量
        {
            buffer(b); // 逐个添加
        }
    }

    // 刷新当前缓冲区
    void flush()
    {
        if (!buffer_.empty()) // 如果缓冲区不为空
        {
            output_.emplace_back(std::move(buffer_)); // 移动到输出
            buffer_.clear();                          // 清空缓冲区
        }
    }

    // 获取输出字符串向量
    const std::vector<std::string> &output()
    {
        flush();        // 刷新缓冲区
        return output_; // 返回输出
    }
};

template <typename T>
std::vector<std::string> serialize(const T &obj)
{
    Serializer s;      // 创建 Serializer 实例
    obj.serialize(s);  // 调用对象的序列化方法
    return s.output(); // 返回输出
}

template <typename T, typename... Targs>
bool parse(T &obj, const std::vector<std::string> &buffers, Targs &&...Fargs)
{
    Parser p{buffers};                           // 创建 Parser 实例
    obj.parse(p, std::forward<Targs>(Fargs)...); // 调用对象的解析方法
    return !p.has_error();                       // 返回是否有错误
}

#endif // PARSER_H
