#ifndef TCP_MINNOW_SOCKET_H
#define TCP_MINNOW_SOCKET_H

#include <iostream>            // 包含输入输出流的头文件
#include <cstddef>             // 包含 cstddef 头文件
#include <exception>           // 包含异常处理的头文件
#include <stdexcept>           // 包含标准异常的头文件
#include <string>              // 包含字符串处理的头文件
#include <sys/socket.h>        // 包含 socket 相关的系统调用
#include <sys/syscall.h>       // 包含系统调用的定义
#include <sys/types.h>         // 包含数据类型的定义
#include <unistd.h>            // 包含 POSIX 操作系统 API
#include <utility>             // 包含一些实用的模板函数
#include <atomic>              // 包含原子操作的定义
#include <cstdint>             // 包含固定宽度整数类型的定义
#include <optional>            // 包含可选类型的定义
#include <thread>              // 包含线程的定义
#include <vector>              // 包含向量的定义
#include "byte_stream.h"       // 包含字节流的定义
#include "eventloop.h"         // 包含事件循环的定义
#include "file_descriptor.h"   // 包含文件描述符的定义
#include "socket.h"            // 包含套接字的定义
#include "tcp_config.h"        // 包含 TCP 配置的定义
#include "tcp_peer.h"          // 包含 TCP 对等体的定义
#include "tuntap_adapter.h"    // 包含 TUN/TAP 适配器的定义
#include "parser.h"            // 包含解析器的头文件
#include "tun.h"               // 包含 TUN 设备的头文件
#include "exception.h"         // 包含异常处理的头文件

// 定义 TCP 轮询的时间间隔（毫秒）
static constexpr size_t TCP_TICK_MS = 10;

// 获取当前时间戳（毫秒）
inline uint64_t timestamp_ms()
{
    // 确保 steady_clock 的 duration 是以纳秒为单位
    static_assert(std::is_same<std::chrono::steady_clock::duration, std::chrono::nanoseconds>::value);

    // 返回自纪元以来的毫秒数
    return std::chrono::steady_clock::now().time_since_epoch().count() / 1000000;
}

// 定义一个模板类 TCPMinnowSocket，适用于任何符合 TCPDatagramAdapter 概念的类型
template <TCPDatagramAdapter AdaptT>
class TCPMinnowSocket : public LocalStreamSocket
{
public:
    // 构造函数，接受一个适配器接口，用于读取和写入数据报
    explicit TCPMinnowSocket(AdaptT &&datagram_interface);

    // 关闭套接字，并等待 TCPPeer 线程完成
    void wait_until_closed();

    // 使用指定的配置进行连接，阻塞直到连接成功或失败
    void connect(const TCPConfig &c_tcp, const FdAdapterConfig &c_ad);

    // 使用指定的配置进行监听和接受，阻塞直到接受成功或失败
    void listen_and_accept(const TCPConfig &c_tcp, const FdAdapterConfig &c_ad);

    // 析构函数，当连接的套接字被析构时，会发送 RST
    ~TCPMinnowSocket();

    // 禁止拷贝构造、移动构造、拷贝赋值和移动赋值
    TCPMinnowSocket(const TCPMinnowSocket &) = delete;
    TCPMinnowSocket(TCPMinnowSocket &&) = delete;
    TCPMinnowSocket &operator=(const TCPMinnowSocket &) = delete;
    TCPMinnowSocket &operator=(TCPMinnowSocket &&) = delete;

    // 禁止绑定地址、获取本地地址和设置地址重用
    void bind(const Address &address) = delete;
    Address local_address() const = delete;
    void set_reuseaddr() = delete;

    // 返回对等地址
    const Address &peer_address() const { return _datagram_adapter.config().destination; }

protected:
    AdaptT _datagram_adapter; // 用于底层数据报套接字的适配器

private:
    LocalStreamSocket _thread_data; // 用于所有者和 TCP 线程之间读写的流套接字

    // 初始化 TCPPeer 和事件循环
    void _initialize_TCP(const TCPConfig &config);

    std::optional<TCPPeer> _tcp{}; // TCP 对等体的可选实例

    EventLoop _eventloop{}; // 处理所有事件的事件循环

    // 在指定条件为真时处理事件
    void _tcp_loop(const std::function<bool()> &condition);

    // TCPPeer 线程的主循环
    void _tcp_main();

    std::thread _tcp_thread{}; // TCPPeer 线程的句柄，所有者线程在析构函数中调用 join()

    // 从套接字对构造 LocalStreamSocket 文件描述符，初始化事件循环
    TCPMinnowSocket(std::pair<FileDescriptor, FileDescriptor> data_socket_pair, AdaptT &&datagram_interface);

    std::atomic_bool _abort{false}; // 用于强制 TCPPeer 线程关闭的标志

    bool _inbound_shutdown{false}; // TCPMinnowSocket 是否关闭了传入数据

    bool _outbound_shutdown{false}; // 所有者是否关闭了传出数据

    bool _fully_acked{false}; // 传出的数据是否已被对等体完全确认
};

// 定义 TCPOverIPv4MinnowSocket 和 LossyTCPOverIPv4MinnowSocket 类型
using TCPOverIPv4MinnowSocket = TCPMinnowSocket<TCPOverIPv4OverTunFdAdapter>;
using LossyTCPOverIPv4MinnowSocket = TCPMinnowSocket<LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>>;

// 辅助类 TINYTCPSocket，使 TCPOverIPv4MinnowSocket 的行为更接近于内核 TCPSocket
class TINYTCPSocket : public TCPOverIPv4MinnowSocket
{
public:
    // 构造函数，初始化 TCPOverIPv4MinnowSocket，创建一个 TUN 设备
    TINYTCPSocket() : TCPOverIPv4MinnowSocket(TCPOverIPv4OverTunFdAdapter{TunFD{"tun144"}}) {}

    // 连接到指定地址
    void connect(const Address &address)
    {
        TCPConfig tcp_config;        // 创建 TCP 配置对象
        tcp_config.rt_timeout = 100; // 设置重传超时

        FdAdapterConfig multiplexer_config;                                                              // 创建文件描述符适配器配置
        multiplexer_config.source = {"169.254.144.9", std::to_string(uint16_t(std::random_device()()))}; // 设置源地址和端口
        multiplexer_config.destination = address;                                                        // 设置目标地址

        // 调用基类的 connect 方法
        TCPOverIPv4MinnowSocket::connect(tcp_config, multiplexer_config);
    }
};

// TCPMinnowSocket 类的 TCP 循环
template <TCPDatagramAdapter AdaptT>
void TCPMinnowSocket<AdaptT>::_tcp_loop(const std::function<bool()> &condition)
{
    auto base_time = timestamp_ms(); // 获取初始时间戳
    while (condition())
    {                                                       // 当条件为真时循环
        auto ret = _eventloop.wait_next_event(TCP_TICK_MS); // 等待下一个事件
        if (ret == EventLoop::Result::Exit || _abort)
        {          // 如果退出或中止
            break; // 退出循环
        }

        // 检查 TCP 连接是否已初始化
        if (!_tcp.has_value())
        {
            throw std::runtime_error("_tcp_loop entered before TCPPeer initialized");
        }

        // 如果 TCP 连接处于活动状态
        if (_tcp.value().active())
        {
            const auto next_time = timestamp_ms();                                                  // 获取当前时间戳
            _tcp.value().tick(next_time - base_time, [&](auto x) { _datagram_adapter.write(x); });  // 调用 tick 方法处理 TCP 逻辑
            _datagram_adapter.tick(next_time - base_time);                                          // 更新数据报适配器
            base_time = next_time;                                                                  // 更新基准时间
        }
    }
}

// TCPMinnowSocket 构造函数
template <TCPDatagramAdapter AdaptT>
TCPMinnowSocket<AdaptT>::TCPMinnowSocket(std::pair<FileDescriptor, FileDescriptor> data_socket_pair,
                                         AdaptT &&datagram_interface)
    : LocalStreamSocket(std::move(data_socket_pair.first)), // 初始化基类 LocalStreamSocket
      _datagram_adapter(std::move(datagram_interface)),     // 初始化数据报适配器
      _thread_data(std::move(data_socket_pair.second))
{                                     // 初始化线程数据
    _thread_data.set_blocking(false); // 设置线程数据为非阻塞
    set_blocking(false);              // 设置当前套接字为非阻塞
}

// 初始化 TCP 连接
template <TCPDatagramAdapter AdaptT>
void TCPMinnowSocket<AdaptT>::_initialize_TCP(const TCPConfig &config)
{
    _tcp.emplace(config); // 初始化 TCP 连接

    // 设置事件循环
    // 处理三种事件：
    // 1) 接收到的传入数据报（需要传递给 TCPPeer::receive 方法）
    // 2) 从本地应用程序通过 write() 调用接收到的出站字节
    // 3) 由重组器重新组装的传入字节（需要从 inbound_stream 读取并写回本地流套接字）

    // 规则 1：从过滤的包流中读取并转发到 TCPConnection
    _eventloop.add_rule(
        "receive TCP segment from the network",
        _datagram_adapter.fd(), // 数据报适配器的文件描述符
        Direction::In,          // 输入方向
        [&]
        {
            if (auto seg = _datagram_adapter.read())
            { // 读取数据报
                _tcp->receive(std::move(seg.value()), [&](auto x) { _datagram_adapter.write(x); });
            }

            // 调试输出
            if (_thread_data.eof() && _tcp.value().sender().sequence_numbers_in_flight() == 0 && !_fully_acked)
            {
                std::cerr << "DEBUG: minnow outbound stream to " << _datagram_adapter.config().destination.to_string();
                std::cerr << " has been fully acknowledged.\n";
                _fully_acked = true; // 标记为已完全确认
            }
        },
        [&]
        { return _tcp->active(); } // 仅在 TCP 连接处于活动状态时执行
    );

    // 规则 2：从管道读取到出站缓冲区
    _eventloop.add_rule(
        "push bytes to TCPPeer",
        _thread_data,  // 线程数据
        Direction::In, // 输入方向
        [&]
        {
            std::string data;                                          // 存储读取的数据
            data.resize(_tcp->outbound_writer().available_capacity()); // 调整大小以适应可用容量
            _thread_data.read(data);                                   // 从线程数据读取
            _tcp->outbound_writer().push(std::move(data));             // 推送到 TCP 的出站写入器

            if (_thread_data.eof())
            {                                    // 如果到达文件末尾
                _tcp->outbound_writer().close(); // 关闭写入器
                _outbound_shutdown = true;       // 标记为出站关闭

                // 调试输出
                std::cerr << "DEBUG: minnow outbound stream to " << _datagram_adapter.config().destination.to_string();
                std::cerr << " finished (" << _tcp.value().sender().sequence_numbers_in_flight() << " seqno";
                std::cerr << (_tcp.value().sender().sequence_numbers_in_flight() == 1 ? "" : "s");
                std::cerr << " still in flight).\n";
            }
            _tcp->push([&](auto x) { _datagram_adapter.write(x); }); // 推送数据到数据报适配器
        },
        [&]
        {
            return (_tcp->active()) && (!_outbound_shutdown) && (_tcp->outbound_writer().available_capacity() > 0);
        },
        [&]
        {
            _tcp->outbound_writer().close(); // 关闭写入器
            _outbound_shutdown = true;       // 标记为出站关闭
        },
        [&]
        {
            std::cerr << "DEBUG: minnow outbound stream had error.\n"; // 调试输出错误
            _tcp->outbound_writer().set_error();                       // 设置写入器错误
        });

    // 规则 3：从传入缓冲区读取到管道
    _eventloop.add_rule(
        "read bytes from inbound stream",
        _thread_data,   // 线程数据
        Direction::Out, // 输出方向
        [&]
        {
            Reader &inbound = _tcp->inbound_reader(); // 获取传入读取器
            // 从 inbound_stream 写入到管道，处理部分写入的可能性
            if (inbound.bytes_buffered())
            {                                                          // 如果有缓冲字节
                const std::string_view buffer = inbound.peek();        // 查看缓冲区
                const auto bytes_written = _thread_data.write(buffer); // 写入到线程数据
                inbound.pop(bytes_written);                            // 从读取器中弹出已写入的字节
            }

            // 检查传入流是否完成或有错误
            if (inbound.is_finished() || inbound.has_error())
            {
                _thread_data.shutdown(SHUT_WR); // 关闭写入端
                _inbound_shutdown = true;       // 标记为传入关闭

                // 调试输出
                std::cerr << "DEBUG: minnow inbound stream from " << _datagram_adapter.config().destination.to_string();
                std::cerr << " finished " << (inbound.has_error() ? "uncleanly.\n" : "cleanly.\n");
            }
        },
        [&]
        {
            return _tcp->inbound_reader().bytes_buffered() || ((_tcp->inbound_reader().is_finished() || _tcp->inbound_reader().has_error()) && !_inbound_shutdown);
        },
        [&] {},
        [&]
        {
            std::cerr << "DEBUG: minnow inbound stream had error.\n"; // 调试输出错误
            _tcp->inbound_reader().set_error();                       // 设置读取器错误
        });
}

// 调用 socketpair 并返回指定类型的连接 Unix 域套接字
template <std::derived_from<Socket> SocketType>
inline std::pair<SocketType, SocketType> socket_pair_helper(int domain, int type, int protocol = 0)
{
    std::array<int, 2> fds{};                                                        // 存储文件描述符的数组
    CheckSystemCall("socketpair", ::socketpair(domain, type, protocol, fds.data())); // 创建套接字对
    return {SocketType{FileDescriptor{fds[0]}}, SocketType{FileDescriptor{fds[1]}}}; // 返回连接的套接字对
}

// datagram_interface 是底层接口（例如 UDP、IP 或以太网）
template <TCPDatagramAdapter AdaptT>
TCPMinnowSocket<AdaptT>::TCPMinnowSocket(AdaptT &&datagram_interface)
    : TCPMinnowSocket(socket_pair_helper<LocalStreamSocket>(AF_UNIX, SOCK_STREAM), std::move(datagram_interface)) {}

// TCPMinnowSocket 析构函数
template <TCPDatagramAdapter AdaptT>
TCPMinnowSocket<AdaptT>::~TCPMinnowSocket()
{
    try
    {
        if (_tcp_thread.joinable())
        {                                                                  // 如果 TCP 线程可连接
            std::cerr << "Warning: unclean shutdown of TCPMinnowSocket\n"; // 警告输出
            _abort.store(true);                                            // 设置中止标志
            _tcp_thread.join();                                            // 等待线程结束
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception destructing TCPMinnowSocket: " << e.what() << std::endl; // 输出异常信息
    }
}

// 等待 TCP 连接关闭
template <TCPDatagramAdapter AdaptT>
void TCPMinnowSocket<AdaptT>::wait_until_closed()
{
    shutdown(SHUT_RDWR); // 关闭读写
    if (_tcp_thread.joinable())
    {                                                                // 如果 TCP 线程可连接
        std::cerr << "DEBUG: minnow waiting for clean shutdown... "; // 调试输出
        _tcp_thread.join();                                          // 等待线程结束
        std::cerr << "done.\n";                                      // 完成输出
    }
}

// c_tcp 是 TCP 连接的 TCPConfig
// c_ad 是 FdAdapterConfig 的 FdAdapter
template <TCPDatagramAdapter AdaptT>
void TCPMinnowSocket<AdaptT>::connect(const TCPConfig &c_tcp, const FdAdapterConfig &c_ad)
{
    if (_tcp)
    {                                                                                 // 如果 TCP 已初始化
        throw std::runtime_error("connect() with TCPConnection already initialized"); // 抛出异常
    }

    _initialize_TCP(c_tcp); // 初始化 TCP

    _datagram_adapter.config_mut() = c_ad; // 设置数据报适配器的配置

    std::cerr << "DEBUG: minnow connecting to " << c_ad.destination.to_string() << "...\n"; // 调试输出连接信息

    if (!_tcp.has_value())
    {                                                                     // 检查 TCP 是否成功初始化
        throw std::runtime_error("TCPPeer not successfully initialized"); // 抛出异常
    }

    _tcp->push([&](auto x) { _datagram_adapter.write(x); }); // 推送数据到数据报适配器

    if (_tcp->sender().sequence_numbers_in_flight() != 1)
    {                                                                                                           // 检查序列号
        throw std::runtime_error("After TCPConnection::connect(), expected sequence_numbers_in_flight() == 1"); // 抛出异常
    }

    // 进入 TCP 循环，直到序列号为 1
    _tcp_loop([&] { return _tcp->sender().sequence_numbers_in_flight() == 1; });
    if (_tcp->inbound_reader().has_error())
    {                                                                                                  // 检查是否有错误
        std::cerr << "DEBUG: minnow error on connecting to " << c_ad.destination.to_string() << ".\n"; // 调试输出错误信息
    }
    else
    {
        std::cerr << "DEBUG: minnow successfully connected to " << c_ad.destination.to_string() << ".\n"; // 调试输出成功信息
    }
    _tcp_thread = std::thread(&TCPMinnowSocket::_tcp_main, this); // 启动 TCP 主线程
}

// c_tcp 是 TCP 连接的 TCPConfig
// c_ad 是 FdAdapterConfig 的 FdAdapter
template <TCPDatagramAdapter AdaptT>
void TCPMinnowSocket<AdaptT>::listen_and_accept(const TCPConfig &c_tcp, const FdAdapterConfig &c_ad)
{
    if (_tcp)
    {                                                                                           // 如果 TCP 已初始化
        throw std::runtime_error("listen_and_accept() with TCPConnection already initialized"); // 抛出异常
    }

    _initialize_TCP(c_tcp);                // 初始化 TCP
    _datagram_adapter.config_mut() = c_ad; // 设置数据报适配器的配置
    _datagram_adapter.set_listening(true); // 设置为监听状态

    std::cerr << "DEBUG: minnow listening for incoming connection...\n"; // 调试输出监听信息
    // 进入 TCP 循环，直到没有确认或序列号在等待确认中
    _tcp_loop([&] { return (!_tcp->has_ackno()) || (_tcp->sender().sequence_numbers_in_flight()); });
    std::cerr << "DEBUG: minnow new connection from " << _datagram_adapter.config().destination.to_string() << ".\n"; // 调试输出新连接信息
    _tcp_thread = std::thread(&TCPMinnowSocket::_tcp_main, this);                                                     // 启动 TCP 主线程
}

// TCP 主线程
template <TCPDatagramAdapter AdaptT>
void TCPMinnowSocket<AdaptT>::_tcp_main()
{
    try
    {
        if (!_tcp.has_value())
        {                                       // 检查 TCP 是否已初始化
            throw std::runtime_error("no TCP"); // 抛出异常
        }
        _tcp_loop([] { return true; }); // 进入 TCP 循环
        shutdown(SHUT_RDWR);            // 关闭读写
        if (!_tcp.value().active())
        { // 检查 TCP 连接是否仍然活动
            std::cerr << "DEBUG: minnow TCP connection finished ";
            std::cerr << (_tcp->inbound_reader().has_error() ? "uncleanly.\n" : "cleanly.\n"); // 调试输出连接结束信息
        }
        _tcp.reset(); // 重置 TCP 连接
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in TCPConnection runner thread: " << e.what() << "\n"; // 输出异常信息
        throw e;                                                                       // 重新抛出异常
    }
}

#endif
