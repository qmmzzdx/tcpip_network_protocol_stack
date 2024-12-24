#ifndef SOCKET_H
#define SOCKET_H

#include <cstdint>           // 引入 C++ 标准库中的整数类型
#include <functional>        // 引入 C++ 标准库中的函数对象
#include <sys/socket.h>      // 引入 POSIX 套接字 API
#include "address.h"         // 引入自定义的地址类
#include "file_descriptor.h" // 引入自定义的文件描述符类

// Socket 类，表示一个网络套接字，继承自 FileDescriptor
class Socket : public FileDescriptor
{
private:
    // 获取地址的私有方法，使用函数指针来调用不同的套接字操作
    Address get_address(const std::string &name_of_function, const std::function<int(int, sockaddr *, socklen_t *)> &function) const;

protected:
    // 构造函数，接受域、类型和协议
    Socket(int domain, int type, int protocol = 0);

    // 移动构造函数，接受一个文件描述符
    Socket(FileDescriptor &&fd, int domain, int type, int protocol = 0);

    // 获取套接字选项的模板方法
    template <typename option_type>
    socklen_t getsockopt(int level, int option, option_type &option_value) const;

    // 设置套接字选项的模板方法
    template <typename option_type>
    void setsockopt(int level, int option, const option_type &option_value);

    // 设置套接字选项，接受字符串视图
    void setsockopt(int level, int option, std::string_view option_val);

public:
    // 绑定套接字到指定地址
    void bind(const Address &address);

    // 将套接字绑定到指定的设备
    void bind_to_device(std::string_view device_name);

    // 连接到指定地址
    void connect(const Address &address);

    // 关闭套接字的某些功能
    void shutdown(int how);

    // 获取本地地址
    Address local_address() const;

    // 获取对等地址
    Address peer_address() const;

    // 设置套接字的重用地址选项
    void set_reuseaddr();

    // 检查是否有错误并抛出异常
    void throw_if_error() const;
};

// DatagramSocket 类，表示数据报套接字，继承自 Socket
class DatagramSocket : public Socket
{
private:
    // 使用 Socket 的构造函数
    using Socket::Socket;

public:
    // 接收数据并返回源地址和有效载荷
    void recv(Address &source_address, std::string &payload);

    // 发送数据到指定地址
    void sendto(const Address &destination, std::string_view payload);

    // 发送数据
    void send(std::string_view payload);
};

// UDPSocket 类，表示 UDP 套接字，继承自 DatagramSocket
class UDPSocket : public DatagramSocket
{
private:
    // 移动构造函数，接受一个文件描述符
    explicit UDPSocket(FileDescriptor &&fd) : DatagramSocket(std::move(fd), AF_INET, SOCK_DGRAM) {}

public:
    // 默认构造函数，创建一个 UDP 套接字
    UDPSocket() : DatagramSocket(AF_INET, SOCK_DGRAM) {}
};

// TCPSocket 类，表示 TCP 套接字，继承自 Socket
class TCPSocket : public Socket
{
private:
    // 移动构造函数，接受一个文件描述符
    explicit TCPSocket(FileDescriptor &&fd) : Socket(std::move(fd), AF_INET, SOCK_STREAM, IPPROTO_TCP) {}

public:
    // 默认构造函数，创建一个 TCP 套接字
    TCPSocket() : Socket(AF_INET, SOCK_STREAM) {}

    // 监听传入连接
    void listen(int backlog = 16);

    // 接受传入连接并返回新的 TCPSocket
    TCPSocket accept();
};

// PacketSocket 类，表示数据包套接字，继承自 DatagramSocket
class PacketSocket : public DatagramSocket
{
public:
    // 构造函数，接受类型和协议
    PacketSocket(const int type, const int protocol) : DatagramSocket(AF_PACKET, type, protocol) {}

    // 设置套接字为混杂模式
    void set_promiscuous();
};

// LocalStreamSocket 类，表示本地流套接字，继承自 Socket
class LocalStreamSocket : public Socket
{
public:
    // 移动构造函数，接受一个文件描述符
    explicit LocalStreamSocket(FileDescriptor &&fd) : Socket(std::move(fd), AF_UNIX, SOCK_STREAM) {}
};

// LocalDatagramSocket 类，表示本地数据报套接字，继承自 DatagramSocket
class LocalDatagramSocket : public DatagramSocket
{
private:
    // 移动构造函数，接受一个文件描述符
    explicit LocalDatagramSocket(FileDescriptor &&fd) : DatagramSocket(std::move(fd), AF_UNIX, SOCK_DGRAM) {}

public:
    // 默认构造函数，创建一个本地数据报套接字
    LocalDatagramSocket() : DatagramSocket(AF_UNIX, SOCK_DGRAM) {}
};

#endif // SOCKET_H
