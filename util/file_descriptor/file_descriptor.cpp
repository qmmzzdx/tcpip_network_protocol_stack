#include <iostream>           // 包含输入输出流
#include <fcntl.h>            // 包含文件控制操作的定义
#include <unistd.h>           // 包含 POSIX 操作系统 API
#include <sys/uio.h>          // 包含 iovec 结构的定义
#include <stdexcept>          // 包含标准异常类
#include <sys/stat.h>         // 包含文件状态的定义
#include <sys/types.h>        // 包含数据类型的定义
#include <algorithm>          // 包含算法函数
#include "exception.h"        // 自定义异常类的头文件
#include "file_descriptor.h"  // FileDescriptor 类的头文件

// 检查系统调用的返回值
template <typename T>
T FileDescriptor::FDWrapper::CheckSystemCall(std::string_view s_attempt, T return_value) const
{
    // 如果返回值大于等于 0，表示成功
    if (return_value >= 0)
    {
        return return_value;
    }

    // 如果是非阻塞模式且返回值为 EAGAIN 或 EINPROGRESS，返回 0
    if (non_blocking_ && (errno == EAGAIN || errno == EINPROGRESS))
    {
        return 0;
    }
    // 否则抛出异常
    throw unix_error{s_attempt};
}

// 检查系统调用的返回值，确保 internal_fd_ 存在
template <typename T>
T FileDescriptor::CheckSystemCall(std::string_view s_attempt, T return_value) const
{
    if (!internal_fd_) // 检查 internal_fd_ 是否为空
    {
        throw std::runtime_error("internal error: missing internal_fd_");
    }
    return internal_fd_->CheckSystemCall(s_attempt, return_value);
}

// FDWrapper 构造函数，初始化文件描述符
FileDescriptor::FDWrapper::FDWrapper(int fd) : fd_(fd)
{
    if (fd < 0) // 检查文件描述符是否有效
    {
        throw std::runtime_error("invalid fd number:" + std::to_string(fd));
    }

    // 获取文件描述符的标志
    const int flags = CheckSystemCall("fcntl", fcntl(fd, F_GETFL)); // NOLINT(*-vararg)
    non_blocking_ = flags & O_NONBLOCK;                             // 检查是否为非阻塞模式
}

// 关闭文件描述符
void FileDescriptor::FDWrapper::close()
{
    CheckSystemCall("close", ::close(fd_)); // 关闭文件描述符
    eof_ = closed_ = true;                  // 设置 EOF 和 closed 标志
}

// FDWrapper 析构函数
FileDescriptor::FDWrapper::~FDWrapper()
{
    try
    {
        if (closed_) // 如果已经关闭，直接返回
        {
            return;
        }
        close(); // 否则关闭文件描述符
    }
    catch (const std::exception &e) // 捕获异常并输出错误信息
    {
        std::cerr << "Exception destructing FDWrapper: " << e.what() << std::endl;
    }
}

// FileDescriptor 构造函数，接受文件描述符
FileDescriptor::FileDescriptor(int fd) : internal_fd_(std::make_shared<FDWrapper>(fd)) {}

// FileDescriptor 构造函数，接受共享指针
FileDescriptor::FileDescriptor(std::shared_ptr<FDWrapper> other_shared_ptr) : internal_fd_(std::move(other_shared_ptr)) {}

// 复制当前文件描述符
FileDescriptor FileDescriptor::duplicate() const
{
    return FileDescriptor{internal_fd_};
}

// 从文件描述符读取数据到字符串
void FileDescriptor::read(std::string &buffer)
{
    if (buffer.empty()) // 如果缓冲区为空，调整大小
    {
        buffer.resize(kReadBufferSize);
    }

    // 从文件描述符读取数据
    const ssize_t bytes_read = ::read(fd_num(), buffer.data(), buffer.size());
    if (bytes_read < 0) // 检查读取是否成功
    {
        if (internal_fd_->non_blocking_ && (errno == EAGAIN || errno == EINPROGRESS))
        {
            buffer.clear(); // 如果是非阻塞模式且没有数据，清空缓冲区
            return;
        }
        throw unix_error{"read"}; // 否则抛出异常
    }

    register_read(); // 注册读取操作

    if (bytes_read == 0) // 如果读取到 EOF
    {
        internal_fd_->eof_ = true; // 设置 EOF 标志
    }

    if (bytes_read > static_cast<ssize_t>(buffer.size())) // 检查读取的字节数是否超过缓冲区大小
    {
        throw std::runtime_error("read() read more than requested");
    }

    buffer.resize(bytes_read); // 调整缓冲区大小
}

// 从文件描述符读取数据到字符串向量
void FileDescriptor::read(std::vector<std::string> &buffers)
{
    if (buffers.empty()) // 如果缓冲区为空，直接返回
    {
        return;
    }

    buffers.back().clear();                 // 清空最后一个缓冲区
    buffers.back().resize(kReadBufferSize); // 调整大小

    std::vector<iovec> iovecs;      // 创建 iovec 结构的向量
    iovecs.reserve(buffers.size()); // 预留空间
    size_t total_size = 0;          // 记录总大小
    for (const auto &x : buffers)   // 遍历缓冲区
    {
        iovecs.push_back({const_cast<char *>(x.data()), x.size()}); // 将每个缓冲区添加到 iovec
        total_size += x.size();                                     // 累加总大小
    }

    // 使用 readv 进行读取
    const ssize_t bytes_read = ::readv(fd_num(), iovecs.data(), static_cast<int>(iovecs.size()));
    if (bytes_read < 0) // 检查读取是否成功
    {
        if (internal_fd_->non_blocking_ && (errno == EAGAIN || errno == EINPROGRESS))
        {
            buffers.clear(); // 如果是非阻塞模式且没有数据，清空缓冲区
            return;
        }
        throw unix_error{"read"}; // 否则抛出异常
    }

    register_read(); // 注册读取操作

    if (bytes_read > static_cast<ssize_t>(total_size)) // 检查读取的字节数是否超过总大小
    {
        throw std::runtime_error("read() read more than requested");
    }

    size_t remaining_size = bytes_read; // 剩余字节数
    for (auto &buf : buffers)           // 遍历缓冲区
    {
        if (remaining_size >= buf.size()) // 如果剩余字节数大于当前缓冲区大小
        {
            remaining_size -= buf.size(); // 减去当前缓冲区大小
        }
        else // 否则调整当前缓冲区大小
        {
            buf.resize(remaining_size);
            remaining_size = 0; // 设置剩余字节数为 0
        }
    }
}

// 写入数据到文件描述符
size_t FileDescriptor::write(std::string_view buffer)
{
    return write(std::vector<std::string_view>{buffer}); // 将字符串视图转换为字符串向量并调用写入方法
}

// 写入字符串向量到文件描述符
size_t FileDescriptor::write(const std::vector<std::string> &buffers)
{
    std::vector<std::string_view> views; // 创建字符串视图的向量
    views.reserve(buffers.size());       // 预留空间
    for (const auto &x : buffers)        // 遍历字符串向量
    {
        views.push_back(x); // 将每个字符串添加到视图向量
    }
    return write(views); // 调用写入方法
}

// 写入字符串视图向量到文件描述符
size_t FileDescriptor::write(const std::vector<std::string_view> &buffers)
{
    std::vector<iovec> iovecs;      // 创建 iovec 结构的向量
    iovecs.reserve(buffers.size()); // 预留空间
    size_t total_size = 0;          // 记录总大小
    for (const auto x : buffers)    // 遍历字符串视图向量
    {
        iovecs.push_back({const_cast<char *>(x.data()), x.size()}); // 将每个字符串视图添加到 iovec
        total_size += x.size();                                     // 累加总大小
    }

    // 使用 writev 进行写入
    const ssize_t bytes_written = CheckSystemCall("writev", ::writev(fd_num(), iovecs.data(), static_cast<int>(iovecs.size())));
    register_write(); // 注册写入操作

    if (bytes_written == 0 && total_size != 0) // 检查写入是否成功
    {
        throw std::runtime_error("write returned 0 given non-empty input buffer");
    }

    if (bytes_written > static_cast<ssize_t>(total_size)) // 检查写入的字节数是否超过总大小
    {
        throw std::runtime_error("write wrote more than length of input buffer");
    }
    return bytes_written; // 返回写入的字节数
}

// 设置文件描述符的阻塞模式
void FileDescriptor::set_blocking(bool blocking)
{
    int flags = CheckSystemCall("fcntl", fcntl(fd_num(), F_GETFL)); // 获取当前文件描述符的标志
    if (blocking)
    {
        flags ^= (flags & O_NONBLOCK); // 如果设置为阻塞模式，清除非阻塞标志
    }
    else
    {
        flags |= O_NONBLOCK; // 否则设置为非阻塞模式
    }

    CheckSystemCall("fcntl", fcntl(fd_num(), F_SETFL, flags)); // 设置新的标志

    internal_fd_->non_blocking_ = !blocking; // 更新内部状态
}
