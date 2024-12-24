#ifndef FILE_DESCRIPTOR_H
#define FILE_DESCRIPTOR_H

#include <cstddef> // 包含 size_t 类型
#include <limits>  // 包含数值限制
#include <memory>  // 包含智能指针
#include <vector>  // 包含向量类

// FileDescriptor 类用于封装文件描述符的操作
class FileDescriptor
{
    // FDWrapper 类用于管理文件描述符的具体实现
    class FDWrapper
    {
    public:
        int fd_;                    // 文件描述符
        bool eof_ = false;          // 是否到达文件末尾
        bool closed_ = false;       // 是否已关闭
        bool non_blocking_ = false; // 是否为非阻塞模式
        unsigned read_count_ = 0;   // 读取次数
        unsigned write_count_ = 0;  // 写入次数

        // 构造函数，接受文件描述符
        explicit FDWrapper(int fd);
        ~FDWrapper(); // 析构函数，负责关闭文件描述符

        void close(); // 关闭文件描述符

        // 检查系统调用的返回值
        template <typename T>
        T CheckSystemCall(std::string_view s_attempt, T return_value) const;

        // 禁止拷贝构造和拷贝赋值
        FDWrapper(const FDWrapper &other) = delete;
        FDWrapper &operator=(const FDWrapper &other) = delete;
        // 禁止移动构造和移动赋值
        FDWrapper(FDWrapper &&other) = delete;
        FDWrapper &operator=(FDWrapper &&other) = delete;
    };

    std::shared_ptr<FDWrapper> internal_fd_; // 使用智能指针管理 FDWrapper 的生命周期

    // 私有构造函数，接受共享指针
    explicit FileDescriptor(std::shared_ptr<FDWrapper> other_shared_ptr);

protected:
    static constexpr size_t kReadBufferSize = 16384; // 读取缓冲区大小

    // 设置 EOF 标志
    void set_eof() { internal_fd_->eof_ = true; }
    // 注册读取操作
    void register_read() { ++internal_fd_->read_count_; }
    // 注册写入操作
    void register_write() { ++internal_fd_->write_count_; }

    // 检查系统调用的返回值
    template <typename T>
    T CheckSystemCall(std::string_view s_attempt, T return_value) const;

public:
    // 构造函数，接受文件描述符
    explicit FileDescriptor(int fd);

    ~FileDescriptor() = default; // 默认析构函数

    // 读取数据到字符串
    void read(std::string &buffer);
    // 读取数据到字符串向量
    void read(std::vector<std::string> &buffers);

    // 写入数据，接受字符串视图
    size_t write(std::string_view buffer);
    // 写入数据，接受字符串视图向量
    size_t write(const std::vector<std::string_view> &buffers);
    // 写入数据，接受字符串向量
    size_t write(const std::vector<std::string> &buffers);

    // 关闭文件描述符
    void close() { internal_fd_->close(); }

    // 复制当前文件描述符
    FileDescriptor duplicate() const;

    // 设置阻塞模式
    void set_blocking(bool blocking);

    // 获取文件大小
    off_t size() const;

    // 获取文件描述符的数字
    int fd_num() const { return internal_fd_->fd_; }
    // 检查是否到达文件末尾
    bool eof() const { return internal_fd_->eof_; }
    // 检查文件描述符是否已关闭
    bool closed() const { return internal_fd_->closed_; }
    // 获取读取次数
    unsigned int read_count() const { return internal_fd_->read_count_; }
    // 获取写入次数
    unsigned int write_count() const { return internal_fd_->write_count_; }

    // 禁止拷贝构造和拷贝赋值
    FileDescriptor(const FileDescriptor &other) = delete;
    FileDescriptor &operator=(const FileDescriptor &other) = delete;
    // 允许移动构造和移动赋值
    FileDescriptor(FileDescriptor &&other) = default;
    FileDescriptor &operator=(FileDescriptor &&other) = default;
};

#endif // FILE_DESCRIPTOR_H
