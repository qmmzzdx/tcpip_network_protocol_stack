#include <cstddef>           // 引入 C++ 标准库中的大小类型
#include <net/if.h>          // 引入网络接口相关的定义
#include <stdexcept>         // 引入标准异常类
#include <sys/ioctl.h>       // 引入 I/O 控制相关的定义
#include <unistd.h>          // 引入 POSIX 操作系统 API
#include <linux/if_packet.h> // 引入数据包套接字相关的定义
#include "socket.h"          // 引入自定义的 Socket 类定义
#include "exception.h"       // 引入自定义的异常处理类

// Socket 类的构造函数，创建一个套接字
Socket::Socket(const int domain, const int type, const int protocol)
    : FileDescriptor(::CheckSystemCall("socket", socket(domain, type, protocol))) // 调用 socket 函数并检查错误
{
}

// 移动构造函数，接受一个文件描述符并验证其属性
Socket::Socket(FileDescriptor &&fd, int domain, int type, int protocol) // NOLINT(*-swappable-parameters)
    : FileDescriptor(std::move(fd))                                      // 移动文件描述符
{
    int actual_value{};
    socklen_t len{};

    // 验证域
    len = getsockopt(SOL_SOCKET, SO_DOMAIN, actual_value);
    if ((len != sizeof(actual_value)) || (actual_value != domain))
    {
        throw std::runtime_error("socket domain mismatch"); // 抛出异常
    }

    // 验证类型
    len = getsockopt(SOL_SOCKET, SO_TYPE, actual_value);
    if ((len != sizeof(actual_value)) || (actual_value != type))
    {
        throw std::runtime_error("socket type mismatch"); // 抛出异常
    }

    // 验证协议
    len = getsockopt(SOL_SOCKET, SO_PROTOCOL, actual_value);
    if ((len != sizeof(actual_value)) || (actual_value != protocol))
    {
        throw std::runtime_error("socket protocol mismatch"); // 抛出异常
    }
}

// 获取地址的私有方法，使用函数指针来调用不同的套接字操作
Address Socket::get_address(const std::string &name_of_function, const std::function<int(int, sockaddr *, socklen_t *)> &function) const
{
    Address::Raw address;             // 存储原始地址
    socklen_t size = sizeof(address); // 地址大小

    // 调用指定的函数获取地址
    CheckSystemCall(name_of_function, function(fd_num(), address, &size));

    return Address{address, size}; // 返回地址对象
}

// 获取本地地址
Address Socket::local_address() const
{
    return get_address("getsockname", getsockname);
}

// 获取对等地址
Address Socket::peer_address() const
{
    return get_address("getpeername", getpeername);
}

// 绑定套接字到指定地址
void Socket::bind(const Address &address)
{
    CheckSystemCall("bind", ::bind(fd_num(), address.raw(), address.size()));
}

// 将套接字绑定到指定的设备
void Socket::bind_to_device(const std::string_view device_name)
{
    setsockopt(SOL_SOCKET, SO_BINDTODEVICE, device_name);
}

// 连接到指定地址
void Socket::connect(const Address &address)
{
    CheckSystemCall("connect", ::connect(fd_num(), address.raw(), address.size()));
}

// 关闭套接字的某些功能
void Socket::shutdown(const int how)
{
    CheckSystemCall("shutdown", ::shutdown(fd_num(), how)); // 调用 shutdown 函数
    switch (how)
    {
    case SHUT_RD: // 关闭读
        register_read();
        break;
    case SHUT_WR: // 关闭写
        register_write();
        break;
    case SHUT_RDWR: // 关闭读写
        register_read();
        register_write();
        break;
    default:
        throw std::runtime_error("Socket::shutdown() called with invalid `how`"); // 抛出异常
    }
}

// 接收数据并返回源地址和有效载荷
void DatagramSocket::recv(Address &source_address, std::string &payload)
{
    Address::Raw datagram_source_address;                // 存储源地址
    socklen_t fromlen = sizeof(datagram_source_address); // 地址长度

    payload.clear();                 // 清空有效载荷
    payload.resize(kReadBufferSize); // 调整有效载荷大小

    // 接收数据
    const ssize_t recv_len = CheckSystemCall("recvfrom", ::recvfrom(fd_num(), payload.data(), payload.size(), MSG_TRUNC, datagram_source_address, &fromlen));

    // 检查接收的数据是否超出缓冲区
    if (recv_len > static_cast<ssize_t>(payload.size()))
    {
        throw std::runtime_error("recvfrom (oversized datagram)"); // 抛出异常
    }

    register_read();                                     // 注册读事件
    source_address = {datagram_source_address, fromlen}; // 设置源地址
    payload.resize(recv_len);                            // 调整有效载荷大小
}

// 发送数据到指定地址
void DatagramSocket::sendto(const Address &destination, const std::string_view payload)
{
    CheckSystemCall("sendto", ::sendto(fd_num(), payload.data(), payload.length(), 0, destination.raw(), destination.size()));
    register_write(); // 注册写事件
}

// 发送数据
void DatagramSocket::send(const std::string_view payload)
{
    CheckSystemCall("send", ::send(fd_num(), payload.data(), payload.length(), 0));
    register_write(); // 注册写事件
}

// 监听传入连接
void TCPSocket::listen(const int backlog)
{
    CheckSystemCall("listen", ::listen(fd_num(), backlog));
}

// 接受传入连接并返回新的 TCPSocket
TCPSocket TCPSocket::accept()
{
    register_read(); // 注册读事件
    return TCPSocket(FileDescriptor(CheckSystemCall("accept", ::accept(fd_num(), nullptr, nullptr))));
}

// 获取套接字选项的模板方法
template <typename option_type>
socklen_t Socket::getsockopt(const int level, const int option, option_type &option_value) const
{
    socklen_t optlen = sizeof(option_value); // 获取选项值的大小
    CheckSystemCall("getsockopt", ::getsockopt(fd_num(), level, option, &option_value, &optlen));
    return optlen; // 返回选项长度
}

// 设置套接字选项的模板方法
template <typename option_type>
void Socket::setsockopt(const int level, const int option, const option_type &option_value)
{
    CheckSystemCall("setsockopt", ::setsockopt(fd_num(), level, option, &option_value, sizeof(option_value)));
}

// 设置套接字选项，接受字符串视图
void Socket::setsockopt(const int level, const int option, const std::string_view option_val)
{
    CheckSystemCall("setsockopt", ::setsockopt(fd_num(), level, option, option_val.data(), option_val.size()));
}

// 设置套接字的重用地址选项
void Socket::set_reuseaddr()
{
    setsockopt(SOL_SOCKET, SO_REUSEADDR, int{true});
}

// 检查是否有错误并抛出异常
void Socket::throw_if_error() const
{
    int socket_error = 0;                                                 // 存储套接字错误
    const socklen_t len = getsockopt(SOL_SOCKET, SO_ERROR, socket_error); // 获取套接字错误
    if (len != sizeof(socket_error))
    {
        throw std::runtime_error("unexpected length from getsockopt: " + std::to_string(len)); // 抛出异常
    }
    if (socket_error)
    {
        throw unix_error("socket error", socket_error); // 抛出 UNIX 错误
    }
}

// 设置数据包套接字为混杂模式，混杂模式允许网络接口接收所有经过的数据包，而不仅仅是发送到本地地址的数据包。
void PacketSocket::set_promiscuous()
{
    setsockopt(SOL_PACKET, PACKET_ADD_MEMBERSHIP, packet_mreq{local_address().as<sockaddr_ll>()->sll_ifindex, PACKET_MR_PROMISC, {}, {}});
}
