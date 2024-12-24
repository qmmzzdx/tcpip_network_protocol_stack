#include "byte_stream.h"

/*
 *   NOLINT表示不进行静态分析, 用于告诉静态分析工具(如Clang-Tidy, Cppcheck等)忽略当前行或某个范围的代码
 *   (*-downcast)指的是特定的代码检查规则, downcast通常指向下类型转换, 即从基类类型向子类类型转换, 通常是不安全的
 *   NOLINT(*-downcast)是一种指令, 告诉静态分析工具不要对包含向下类型转换的代码进行警告或报告
*/

void read(Reader& reader, uint64_t len, std::string& out)
{
    out.clear();
    while (reader.bytes_buffered() && out.size() < len)
    {
        auto view = reader.peek();
        if (view.empty())
        {
            throw std::runtime_error("Reader::peek() returned empty string_view");
        }
        view = view.substr(0, len - out.size()); // 确保返回的字节数不超过流中能够容纳的最大容量
        out += view;
        reader.pop(view.size());
    }
}

Reader& ByteStream::reader()
{
    static_assert(sizeof(Reader) == sizeof(ByteStream),
        "Please add member variables to the ByteStream base, not the ByteStream Reader.");
    return static_cast<Reader&>(*this); // NOLINT(*-downcast)
}

const Reader& ByteStream::reader() const
{
    static_assert(sizeof(Reader) == sizeof(ByteStream),
        "Please add member variables to the ByteStream base, not the ByteStream Reader.");
    return static_cast<const Reader&>(*this); // NOLINT(*-downcast)
}

Writer& ByteStream::writer()
{
    static_assert(sizeof(Writer) == sizeof(ByteStream),
        "Please add member variables to the ByteStream base, not the ByteStream Writer.");
    return static_cast<Writer&>(*this); // NOLINT(*-downcast)
}

const Writer& ByteStream::writer() const
{
    static_assert(sizeof(Writer) == sizeof(ByteStream),
        "Please add member variables to the ByteStream base, not the ByteStream Writer.");
    return static_cast<const Writer&>(*this); // NOLINT(*-downcast)
}

ByteStream::ByteStream(uint64_t capacity) : capacity_(capacity) {}

bool Writer::is_closed() const
{
    return closed_;
}

void Writer::push(std::string data)
{
    if (!is_closed() && available_capacity() > 0 && !data.empty())
    {
        uint64_t len = std::min(static_cast<uint64_t>(data.size()), available_capacity());
        bytes_stream_.emplace_back(data.substr(0, len)), bytes_pushed_ += len, bytes_buffered_ += len;
    }
}

void Writer::close()
{
    closed_ = true;
}

uint64_t Writer::available_capacity() const
{
    return capacity_ - bytes_buffered_;
}

uint64_t Writer::bytes_pushed() const
{
    return bytes_pushed_;
}

bool Reader::is_finished() const
{
    return closed_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_popped() const
{
    return bytes_popped_;
}

std::string_view Reader::peek() const
{
    return bytes_buffered() > 0 ? std::string_view(bytes_stream_.front()).substr(remove_prefix_len_) : std::string_view();
}

void Reader::pop(uint64_t len)
{
    bytes_popped_ += len, bytes_buffered_ -= len;
    while (len > 0)
    {
        uint64_t sz = static_cast<uint64_t>(bytes_stream_.front().size() - remove_prefix_len_);
        if (len < sz)
        {
            remove_prefix_len_ += len;
            break;
        }
        bytes_stream_.pop_front();
        remove_prefix_len_ = 0, len -= sz;
    }
}

uint64_t Reader::bytes_buffered() const
{
    return bytes_buffered_;
}
