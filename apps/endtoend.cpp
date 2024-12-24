#include <iostream>                    // 引入输入输出流的头文件
#include <cstdlib>                     // 引入 C 标准库的头文件，提供通用工具函数
#include <thread>                      // 引入线程支持的头文件
#include <utility>                     // 引入实用工具的头文件，提供 std::pair 等
#include "exception.h"                 // 引入自定义异常处理的头文件
#include "address.h"                   // 引入地址处理的头文件
#include "router.h"                    // 引入路由器的头文件
#include "arp_message.h"               // 引入 ARP 消息处理的头文件
#include "tcp_over_ip.h"               // 引入 TCP over IP 的头文件
#include "tcp_minnow_socket.h"         // 引入 Minnow socket 实现的头文件
#include "bidirectional_stream_copy.h" // 引入双向流复制的头文件

// 生成随机的以太网地址
EthernetAddress random_host_ethernet_address()
{
    EthernetAddress addr; // 创建以太网地址对象
    for (auto &byte : addr)
    {
        byte = std::random_device()(); // 使用随机设备生成随机字节
    }
    addr.at(0) |= 0x02; // 设置地址的第一个字节的最低两位为 "10"，标记为私有以太网地址
    addr.at(0) &= 0xfe; // 清除第一个字节的最低位

    return addr; // 返回生成的以太网地址
}

// 生成随机的路由器以太网地址
EthernetAddress random_router_ethernet_address()
{
    EthernetAddress addr; // 创建以太网地址对象
    for (auto &byte : addr)
    {
        byte = std::random_device()(); // 使用随机设备生成随机字节
    }
    addr.at(0) = 0x02; // 设置地址的第一个字节的最低两位为 "10"，标记为私有以太网地址
    addr.at(1) = 0;    // 设置第二个字节为 0
    addr.at(2) = 0;    // 设置第三个字节为 0

    return addr; // 返回生成的以太网地址
}

// 生成以太网帧的摘要信息
std::string summary(const EthernetFrame &frame)
{
    std::string out = frame.header.to_string() + ", payload: "; // 获取帧头的字符串表示
    switch (frame.header.type)
    { // 根据帧头的类型进行处理
    case EthernetHeader::TYPE_IPv4:
    {
        InternetDatagram dgram; // 创建互联网数据报对象
        if (parse(dgram, frame.payload))
        {                                                    // 解析有效载荷为 IPv4 数据报
            out.append("IPv4: " + dgram.header.to_string()); // 添加 IPv4 数据报的头信息
        }
        else
        {
            out.append("bad IPv4 datagram"); // 解析失败，添加错误信息
        }
        break;
    }
    case EthernetHeader::TYPE_ARP:
    {
        ARPMessage arp; // 创建 ARP 消息对象
        if (parse(arp, frame.payload))
        {                                          // 解析有效载荷为 ARP 消息
            out.append("ARP: " + arp.to_string()); // 添加 ARP 消息的字符串表示
        }
        else
        {
            out.append("bad ARP message"); // 解析失败，添加错误信息
        }
        break;
    }
    default:
    {
        out.append("unknown frame type"); // 未知帧类型，添加错误信息
        break;
    }
    }
    return out; // 返回生成的摘要信息
}

// 尝试接收以太网帧
std::optional<EthernetFrame> maybe_receive_frame(FileDescriptor &fd)
{
    std::vector<std::string> strs(3);          // 创建一个字符串向量，用于存储接收到的数据
    strs.at(0).resize(EthernetHeader::LENGTH); // 为以太网头部分配空间
    strs.at(1).resize(IPv4Header::LENGTH);     // 为 IPv4 头部分配空间
    fd.read(strs);                             // 从文件描述符中读取数据

    EthernetFrame frame;                                                        // 创建以太网帧对象
    std::vector<std::string> buffers;                                           // 创建一个字符串向量，用于存储解析后的数据
    std::ranges::transform(strs, std::back_inserter(buffers), std::identity()); // 将读取的数据转换为 buffers
    if (!parse(frame, buffers))
    {
        return {}; // 解析以太网帧失败，返回空值
    }
    return frame; // 返回解析成功的以太网帧
}

// 创建一对套接字
inline std::pair<FileDescriptor, FileDescriptor> make_socket_pair()
{
    std::array<int, 2> fds{};                                                        // 创建一个数组，用于存储套接字文件描述符
    CheckSystemCall("socketpair", ::socketpair(AF_UNIX, SOCK_DGRAM, 0, fds.data())); // 创建 UNIX 域的套接字对
    return {FileDescriptor{fds[0]}, FileDescriptor{fds[1]}};                         // 返回文件描述符对
}

// 网络接口适配器类
class NetworkInterfaceAdapter : public TCPOverIPv4Adapter
{
private:
    struct Sender : public NetworkInterface::OutputPort // 发送器结构
    {
        std::pair<FileDescriptor, FileDescriptor> sockets{make_socket_pair()}; // 套接字对

        // 发送以太网帧
        void transmit(const NetworkInterface &n [[maybe_unused]], const EthernetFrame &x) override
        {
            sockets.first.write(serialize(x)); // 将以太网帧序列化并写入套接字
        }
    };

    std::shared_ptr<Sender> sender_ = std::make_shared<Sender>(); // 发送器的共享指针
    NetworkInterface _interface;                                  // 网络接口对象
    Address _next_hop;                                            // 下一跳地址

public:
    // 构造函数
    NetworkInterfaceAdapter(const Address &ip_address, const Address &next_hop)                                             // NOLINT(*-swappable-*)
        : _interface("network interface adapter", sender_, random_host_ethernet_address(), ip_address), _next_hop(next_hop) // 初始化网络接口并设置下一跳地址
    {
    }

    // 读取 TCP 消息
    std::optional<TCPMessage> read()
    {
        auto frame_opt = maybe_receive_frame(sender_->sockets.first); // 尝试接收以太网帧
        if (!frame_opt)
        {
            return {}; // 如果没有接收到帧，返回空值
        }
        EthernetFrame frame = std::move(frame_opt.value()); // 移动接收到的帧

        // 将帧交给网络接口，获取互联网数据报
        _interface.recv_frame(frame);

        // 尝试将 IPv4 数据报解释为 TCP
        if (_interface.datagrams_received().empty())
        {
            return {}; // 如果没有接收到数据报，返回空值
        }

        InternetDatagram dgram = std::move(_interface.datagrams_received().front()); // 移动第一个数据报
        _interface.datagrams_received().pop();                                       // 移除已处理的数据报
        return unwrap_tcp_in_ip(dgram);                                              // 返回解包后的 TCP 消息
    }

    // 发送 TCP 消息
    void write(const TCPMessage &msg)
    {
        _interface.send_datagram(wrap_tcp_in_ip(msg), _next_hop); // 将 TCP 消息封装为数据报并发送
    }

    // 处理定时器
    void tick(const size_t ms_since_last_tick)
    {
        _interface.tick(ms_since_last_tick); // 更新网络接口
    }

    // 获取网络接口
    NetworkInterface &interface()
    {
        return _interface; // 返回网络接口引用
    }

    // 获取文件描述符
    FileDescriptor &fd()
    {
        return sender_->sockets.first; // 返回发送器的第一个套接字
    }

    // 获取帧文件描述符
    FileDescriptor &frame_fd()
    {
        return sender_->sockets.second; // 返回发送器的第二个套接字
    }
};

// 端到端 TCP 套接字类
class TCPSocketEndToEnd : public TCPMinnowSocket<NetworkInterfaceAdapter>
{
    Address _local_address; // 本地地址

public:
    // 构造函数
    TCPSocketEndToEnd(const Address &ip_address, const Address &next_hop)
        : TCPMinnowSocket<NetworkInterfaceAdapter>(NetworkInterfaceAdapter(ip_address, next_hop)), _local_address(ip_address) {}

    // 连接到指定地址
    void connect(const Address &address)
    {
        FdAdapterConfig multiplexer_config; // 多路复用配置

        _local_address = Address{_local_address.ip(), uint16_t(std::random_device()())}; // 随机生成本地端口
        std::cerr << "DEBUG: Connecting from " << _local_address.to_string() << "...\n"; // 输出调试信息
        multiplexer_config.source = _local_address;                                      // 设置源地址
        multiplexer_config.destination = address;                                        // 设置目标地址

        TCPMinnowSocket<NetworkInterfaceAdapter>::connect({}, multiplexer_config); // 调用基类的连接方法
    }

    // 绑定到指定地址
    void bind(const Address &address)
    {
        if (address.ip() != _local_address.ip())
        {                                                                      // 检查 IP 地址是否匹配
            throw std::runtime_error("Cannot bind to " + address.to_string()); // 抛出异常
        }
        _local_address = Address{_local_address.ip(), address.port()}; // 设置本地地址
    }

    // 监听并接受连接
    void listen_and_accept()
    {
        FdAdapterConfig multiplexer_config;                                                  // 多路复用配置
        multiplexer_config.source = _local_address;                                          // 设置源地址
        TCPMinnowSocket<NetworkInterfaceAdapter>::listen_and_accept({}, multiplexer_config); // 调用基类的监听方法
    }

    // 获取适配器
    NetworkInterfaceAdapter &adapter()
    {
        return _datagram_adapter; // 返回适配器引用
    }
};

// NOLINTBEGIN(*-cognitive-complexity)
// 主程序体
void program_body(bool is_client, const std::string &bounce_host, const std::string &bounce_port, const bool debug)
{
    class FramesOut : public NetworkInterface::OutputPort // 输出端帧类
    {
    public:
        std::queue<EthernetFrame> frames{}; // 存储帧的队列
        void transmit(const NetworkInterface &n [[maybe_unused]], const EthernetFrame &x) override
        {
            frames.push(x); // 将帧推入队列
        }
    };

    auto router_to_host = std::make_shared<FramesOut>();     // 创建路由器到主机的输出端
    auto router_to_internet = std::make_shared<FramesOut>(); // 创建路由器到互联网的输出端

    UDPSocket internet_socket;                        // 创建 UDP 套接字
    Address bounce_address{bounce_host, bounce_port}; // 创建跳转地址

    /* 让跳转器知道我们的位置 */
    internet_socket.sendto(bounce_address, ""); // 发送空数据包到跳转器
    internet_socket.sendto(bounce_address, ""); // 发送空数据包到跳转器
    internet_socket.sendto(bounce_address, ""); // 发送空数据包到跳转器
    internet_socket.connect(bounce_address);    // 连接到跳转器

    /* 设置路由器 */
    Router router; // 创建路由器对象

    unsigned int host_side{};     // 主机侧接口 ID
    unsigned int internet_side{}; // 互联网侧接口 ID

    if (is_client)
    { // 如果是客户端
        host_side = router.add_interface(make_shared<NetworkInterface>(
            "host_side", router_to_host, random_router_ethernet_address(), Address{"192.168.0.1"})); // 添加主机侧接口
        internet_side = router.add_interface(make_shared<NetworkInterface>(
            "internet side", router_to_internet, random_router_ethernet_address(), Address{"10.0.0.192"})); // 添加互联网侧接口
        router.add_route(Address{"192.168.0.0"}.ipv4_numeric(), 16, {}, host_side);                         // 添加路由
        router.add_route(Address{"10.0.0.0"}.ipv4_numeric(), 8, {}, internet_side);                         // 添加路由
        router.add_route(Address{"172.16.0.0"}.ipv4_numeric(), 12, Address{"10.0.0.172"}, internet_side);   // 添加路由
    }
    else
    { // 如果是服务器
        host_side = router.add_interface(make_shared<NetworkInterface>(
            "host_side", router_to_host, random_router_ethernet_address(), Address{"172.16.0.1"})); // 添加主机侧接口
        internet_side = router.add_interface(make_shared<NetworkInterface>(
            "internet side", router_to_internet, random_router_ethernet_address(), Address{"10.0.0.172"})); // 添加互联网侧接口
        router.add_route(Address{"172.16.0.0"}.ipv4_numeric(), 12, {}, host_side);                          // 添加路由
        router.add_route(Address{"10.0.0.0"}.ipv4_numeric(), 8, {}, internet_side);                         // 添加路由
        router.add_route(Address{"192.168.0.0"}.ipv4_numeric(), 16, Address{"10.0.0.192"}, internet_side);  // 添加路由
    }

    /* 设置客户端 */
    TCPSocketEndToEnd sock = is_client ? TCPSocketEndToEnd{Address{"192.168.0.50"}, Address{"192.168.0.1"}}
                                       : TCPSocketEndToEnd{Address{"172.16.0.100"}, Address{"172.16.0.1"}};

    std::atomic<bool> exit_flag{}; // 退出标志

    /* 设置网络 */
    std::thread network_thread([&]()
    {
        try
        {
            EventLoop event_loop; // 创建事件循环
            // 从主机到路由器的帧
            event_loop.add_rule("frames from host to router", sock.adapter().frame_fd(), Direction::In, [&] {
                auto frame_opt = maybe_receive_frame(sock.adapter().frame_fd()); // 尝试接收帧
                if (!frame_opt) {
                    return; // 如果没有接收到帧，返回
                }
                EthernetFrame frame = std::move(frame_opt.value()); // 移动接收到的帧
                if (debug) {
                    std::cerr << "     Host->router:     " << summary(frame) << "\n"; // 输出调试信息
                }
                router.interface(host_side)->recv_frame(frame); // 将帧交给路由器
                router.route(); // 路由
            });

            // 从路由器到主机的帧
            event_loop.add_rule(
                "frames from router to host",
                sock.adapter().frame_fd(),
                Direction::Out,
                [&] {
                    auto& f = router_to_host; // 获取路由器到主机的输出端
                    if (debug) {
                        std::cerr << "     Router->host:     " << summary(f->frames.front()) << "\n"; // 输出调试信息
                    }
                    sock.adapter().frame_fd().write(serialize(f->frames.front())); // 将帧序列化并写入套接字
                    f->frames.pop(); // 移除已处理的帧
                },
                [&] { return !router_to_host->frames.empty(); } // 检查是否有帧可发送
            );

            // 从路由器到互联网的帧
            event_loop.add_rule(
                "frames from router to Internet",
                internet_socket,
                Direction::Out,
                [&] {
                    auto& f = router_to_internet; // 获取路由器到互联网的输出端
                    if (debug) {
                        std::cerr << "     Router->Internet: " << summary(f->frames.front()) << "\n"; // 输出调试信息
                    }
                    internet_socket.write(serialize(f->frames.front())); // 将帧序列化并写入互联网套接字
                    f->frames.pop(); // 移除已处理的帧
                },
                [&] { return !router_to_internet->frames.empty(); } // 检查是否有帧可发送
            );

            // 从互联网到路由器的帧
            event_loop.add_rule("frames from Internet to router", internet_socket, Direction::In, [&] {
                auto frame_opt = maybe_receive_frame(internet_socket); // 尝试接收帧
                if (!frame_opt) {
                    return; // 如果没有接收到帧，返回
                }
                EthernetFrame frame = std::move(frame_opt.value()); // 移动接收到的帧
                if (debug) {
                    std::cerr << "     Internet->router: " << summary(frame) << "\n"; // 输出调试信息
                }
                router.interface(internet_side)->recv_frame(frame); // 将帧交给路由器
                router.route(); // 路由
            });

            while (true)
            {
                if (EventLoop::Result::Exit == event_loop.wait_next_event(10)) // 等待下一个事件
                {
                    std::cerr << "Exiting...\n"; // 输出退出信息
                    return; // 退出循环
                }
                router.interface(host_side)->tick(10); // 更新主机侧接口
                router.interface(internet_side)->tick(10); // 更新互联网侧接口

                if (exit_flag) // 检查退出标志
                {
                    return;
                }
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Thread ending from exception: " << e.what() << "\n"; // 输出异常信息
        }
    });

    try
    {
        if (is_client)
        {                                                // 如果是客户端
            sock.connect(Address{"172.16.0.100", 1234}); // 连接到服务器
        }
        else
        {                                             // 如果是服务器
            sock.bind(Address{"172.16.0.100", 1234}); // 绑定到指定地址
            sock.listen_and_accept();                 // 监听并接受连接
        }
        bidirectional_stream_copy(sock, "172.16.0.100"); // 进行双向流复制
        sock.wait_until_closed();                        // 等待套接字关闭
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << "\n"; // 输出异常信息
    }

    std::cerr << "Exiting... "; // 输出退出信息
    exit_flag = true;           // 设置退出标志
    network_thread.join();      // 等待网络线程结束
    std::cerr << "done.\n";     // 输出完成信息
}
// NOLINTEND(*-cognitive-complexity)

// 打印用法信息
void print_usage(const std::string &argv0)
{
    std::cerr << "Usage: " << argv0 << " client HOST PORT [debug]\n"; // 客户端用法
    std::cerr << "or     " << argv0 << " server HOST PORT [debug]\n"; // 服务器用法
}

int main(int argc, char *argv[])
{
    try
    {
        if (argc <= 0)
        {
            std::abort();
        }
        auto args = std::span(argv, argc); // 使用 span 处理命令行参数

        if (argc != 4 && argc != 5)
        {                         // 检查参数数量
            print_usage(args[0]); // 打印用法信息
            return EXIT_FAILURE;  // 返回失败状态
        }

        if (std::string_view(args[1]) != "client" && std::string_view(args[1]) != "server")
        {                         // 检查是否为有效的模式
            print_usage(args[0]); // 打印用法信息
            return EXIT_FAILURE;  // 返回失败状态
        }
        program_body(std::string_view(args[1]) == "client", args[2], args[3], argc == 5); // 调用程序主体
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n"; // 输出异常信息
        return EXIT_FAILURE;           // 返回失败状态
    }
    return EXIT_SUCCESS; // 返回成功状态
}
