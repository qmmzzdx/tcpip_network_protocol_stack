#include <iostream>                         // 引入输入输出流库，用于输出信息
#include <list>                             // 引入链表库，提供 std::list 类型
#include <unordered_map>                    // 引入无序映射库，提供 std::unordered_map 类型
#include <utility>                          // 引入实用工具库，提供 std::move 等功能
#include "router.h"                         // 引入路由器相关的自定义头文件
#include "arp_message.h"                    // 引入 ARP 消息相关的自定义头文件
#include "network_interface_test_harness.h" // 引入网络接口测试工具的定义
#include "random.h"                         // 引入随机数生成相关的自定义头文件
using namespace std;

// 生成随机的主机以太网地址
EthernetAddress random_host_ethernet_address()
{
    EthernetAddress addr;
    for (auto &byte : addr)
    {
        byte = random_device()(); // 使用随机设备生成本地以太网地址
    }
    addr.at(0) |= 0x02; // 设置地址的第一个字节的最后两位为 "10"，标记为私有以太网地址
    addr.at(0) &= 0xfe; // 确保第一个字节的最后一位为 0

    return addr; // 返回生成的以太网地址
}

// 生成随机的路由器以太网地址
EthernetAddress random_router_ethernet_address()
{
    EthernetAddress addr;
    for (auto &byte : addr)
    {
        byte = random_device()(); // 使用随机设备生成本地以太网地址
    }
    addr.at(0) = 0x02; // 设置地址的第一个字节的最后两位为 "10"，标记为私有以太网地址
    addr.at(1) = 0;    // 设置第二个字节为 0
    addr.at(2) = 0;    // 设置第三个字节为 0

    return addr; // 返回生成的以太网地址
}

// 将字符串转换为 IP 地址
uint32_t ip(const string &str)
{
    return Address{str}.ipv4_numeric(); // 使用 Address 类将字符串转换为 IPv4 数字格式
}

// 网络段类，继承自 NetworkInterface::OutputPort
class NetworkSegment : public NetworkInterface::OutputPort
{
    vector<weak_ptr<NetworkInterface>> connections_{}; // 存储连接的网络接口

public:
    // 传输数据帧
    void transmit(const NetworkInterface &sender, const EthernetFrame &frame) override
    {
        ranges::for_each(connections_, [&](auto &weak_ref)
                         {
            const shared_ptr<NetworkInterface> interface(weak_ref);
            if (&sender != interface.get()) { // 确保不将帧发送回发送者
                cerr << "Transferring frame from " << sender.name() << " to " << interface->name() << ": "
                     << summary(frame) << "\n"; // 输出传输信息
                interface->recv_frame(frame); // 接收数据帧
            } });
    }

    // 连接网络接口
    void connect(const shared_ptr<NetworkInterface> &interface) { connections_.push_back(interface); }
};

// 主机类
class Host
{
    string _name;                            // 主机名称
    Address _my_address;                     // 主机地址
    shared_ptr<NetworkInterface> _interface; // 网络接口
    Address _next_hop;                       // 下一跳地址

    std::list<InternetDatagram> _expecting_to_receive{}; // 期望接收的数据报列表

    // 检查是否期望接收特定数据报
    bool expecting(const InternetDatagram &expected) const
    {
        return ranges::any_of(_expecting_to_receive, [&expected](const auto &x)
                              { return equal(x, expected); });
    }

    // 移除期望接收的数据报
    void remove_expectation(const InternetDatagram &expected)
    {
        for (auto it = _expecting_to_receive.begin(); it != _expecting_to_receive.end(); ++it)
        {
            if (equal(*it, expected))
            {
                _expecting_to_receive.erase(it); // 从期望列表中移除
                return;
            }
        }
    }

public:
    // 构造函数
    Host(string name, const Address &my_address, const Address &next_hop, shared_ptr<NetworkSegment> network)
        : _name(std::move(name)), _my_address(my_address), _interface(
                                                               make_shared<NetworkInterface>(_name, move(network), random_host_ethernet_address(), _my_address)),
          _next_hop(next_hop)
    {
    }

    // 发送数据报
    InternetDatagram send_to(const Address &destination, const uint8_t ttl = 64)
    {
        InternetDatagram dgram;                                                                        // 创建数据报
        dgram.header.src = _my_address.ipv4_numeric();                                                 // 设置源地址
        dgram.header.dst = destination.ipv4_numeric();                                                 // 设置目标地址
        dgram.payload.emplace_back(string{"Cardinal " + to_string(random_device()() % 1000)});         // 添加负载
        dgram.header.len = static_cast<uint64_t>(dgram.header.hlen) * 4 + dgram.payload.back().size(); // 计算数据报长度
        dgram.header.ttl = ttl;                                                                        // 设置生存时间
        dgram.header.compute_checksum();                                                               // 计算校验和

        cerr << "Host " << _name << " trying to send datagram (with next hop = " << _next_hop.ip()
             << "): " << dgram.header.to_string()
             << +" payload=\"" + Printer::prettify(concat(dgram.payload)) + "\"\n"; // 输出发送信息

        _interface->send_datagram(dgram, _next_hop); // 发送数据报

        return dgram; // 返回发送的数据报
    }

    // 获取主机地址
    const Address &address() { return _my_address; }

    // 获取网络接口
    shared_ptr<NetworkInterface> interface() { return _interface; }

    // 期望接收特定数据报
    void expect(const InternetDatagram &expected) { _expecting_to_receive.push_back(expected); }

    // 获取主机名称
    const string &name() { return _name; }

    // 检查接收到的数据报
    void check()
    {
        while (!_interface->datagrams_received().empty())
        {
            auto &dgram_received = _interface->datagrams_received().front(); // 获取接收到的数据报
            if (!expecting(dgram_received))                               // 检查是否是期望接收的数据报
            {
                throw runtime_error("Host " + _name + " received unexpected Internet datagram: " + dgram_received.header.to_string());
            }
            remove_expectation(dgram_received);     // 移除期望接收的数据报
            _interface->datagrams_received().pop(); // 移除已处理的数据报
        }

        if (!_expecting_to_receive.empty()) // 检查是否有未接收到的期望数据报
        {
            throw runtime_error("Host " + _name + " did NOT receive an expected Internet datagram: " + _expecting_to_receive.front().header.to_string());
        }
    }
};

// 网络类
class Network
{
private:
    Router _router{}; // 路由器实例

    // 创建网络段
    shared_ptr<NetworkSegment> upstream{make_shared<NetworkSegment>()},
        eth0_applesauce{make_shared<NetworkSegment>()}, eth2_cherrypie{make_shared<NetworkSegment>()},
        uun{make_shared<NetworkSegment>()}, hs{make_shared<NetworkSegment>()},
        empty{make_shared<NetworkSegment>()};

    // 路由器接口 ID
    size_t default_id, eth0_id, eth1_id, eth2_id, uun3_id, hs4_id, mit5_id;

    unordered_map<string, Host> _hosts{}; // 存储主机的映射

public:
    // 构造函数
    Network()
        : default_id(_router.add_interface(make_shared<NetworkInterface>("default",
                                                                         upstream,
                                                                         random_router_ethernet_address(),
                                                                         Address{"171.67.76.46"}))),
          eth0_id(_router.add_interface(make_shared<NetworkInterface>("eth0",
                                                                      eth0_applesauce,
                                                                      random_router_ethernet_address(),
                                                                      Address{"10.0.0.1"}))),
          eth1_id(_router.add_interface(make_shared<NetworkInterface>("eth1",
                                                                      empty,
                                                                      random_router_ethernet_address(),
                                                                      Address{"172.16.0.1"}))),
          eth2_id(_router.add_interface(make_shared<NetworkInterface>("eth2",
                                                                      eth2_cherrypie,
                                                                      random_router_ethernet_address(),
                                                                      Address{"192.168.0.1"}))),
          uun3_id(_router.add_interface(make_shared<NetworkInterface>("uun3",
                                                                      uun,
                                                                      random_router_ethernet_address(),
                                                                      Address{"198.178.229.1"}))),
          hs4_id(_router.add_interface(
              make_shared<NetworkInterface>("hs4", hs, random_router_ethernet_address(), Address{"143.195.0.2"}))),
          mit5_id(_router.add_interface(make_shared<NetworkInterface>("mit5",
                                                                      empty,
                                                                      random_router_ethernet_address(),
                                                                      Address{"128.30.76.255"})))
    {
        // 初始化主机
        _hosts.insert(
            {"applesauce", {"applesauce", Address{"10.0.0.2"}, Address{"10.0.0.1"}, eth0_applesauce}});
        _hosts.insert(
            {"default_router", {"default_router", Address{"171.67.76.1"}, Address{"0"}, upstream}});
        _hosts.insert(
            {"cherrypie", {"cherrypie", Address{"192.168.0.2"}, Address{"192.168.0.1"}, eth2_cherrypie}});
        _hosts.insert({"hs_router", {"hs_router", Address{"143.195.0.1"}, Address{"0"}, hs}});
        _hosts.insert({"dm42", {"dm42", Address{"198.178.229.42"}, Address{"198.178.229.1"}, uun}});
        _hosts.insert({"dm43", {"dm43", Address{"198.178.229.43"}, Address{"198.178.229.1"}, uun}});

        // 连接网络接口
        upstream->connect(_router.interface(default_id));
        upstream->connect(host("default_router").interface());

        eth0_applesauce->connect(_router.interface(eth0_id));
        eth0_applesauce->connect(host("applesauce").interface());

        eth2_cherrypie->connect(_router.interface(eth2_id));
        eth2_cherrypie->connect(host("cherrypie").interface());

        uun->connect(_router.interface(uun3_id));
        uun->connect(host("dm42").interface());
        uun->connect(host("dm43").interface());

        hs->connect(_router.interface(hs4_id));
        hs->connect(host("hs_router").interface());

        // 添加路由
        _router.add_route(ip("0.0.0.0"), 0, host("default_router").address(), default_id);
        _router.add_route(ip("10.0.0.0"), 8, {}, eth0_id);
        _router.add_route(ip("172.16.0.0"), 16, {}, eth1_id);
        _router.add_route(ip("192.168.0.0"), 24, {}, eth2_id);
        _router.add_route(ip("198.178.229.0"), 24, {}, uun3_id);
        _router.add_route(ip("143.195.0.0"), 17, host("hs_router").address(), hs4_id);
        _router.add_route(ip("143.195.128.0"), 18, host("hs_router").address(), hs4_id);
        _router.add_route(ip("143.195.192.0"), 19, host("hs_router").address(), hs4_id);
        _router.add_route(ip("128.30.76.255"), 16, Address{"128.30.0.1"}, mit5_id);
    }

    // 模拟网络
    void simulate()
    {
        for (unsigned int i = 0; i < 256; i++)
        {
            _router.route(); // 路由数据包
        }

        for (auto &host : _hosts)
        {
            host.second.check(); // 检查每个主机的接收情况
        }
    }

    // 获取主机
    Host &host(const string &name)
    {
        auto it = _hosts.find(name);
        if (it == _hosts.end())
        {
            throw runtime_error("unknown host: " + name); // 如果主机不存在，抛出异常
        }
        if (it->second.name() != name)
        {
            throw runtime_error("invalid host: " + name); // 如果主机名称不匹配，抛出异常
        }
        return it->second; // 返回主机
    }
};

// 网络模拟器函数
void network_simulator()
{
    const string green = "\033[32;1m"; // 绿色输出
    const string normal = "\033[m";    // 正常输出

    cerr << green << "Constructing network." << normal << "\n"; // 输出网络构建信息

    Network network; // 创建网络实例

    // 测试主机之间的流量
    cout << green << "\n\nTesting traffic between two ordinary hosts (applesauce to cherrypie)..." << normal
         << "\n\n";
    {
        auto dgram_sent = network.host("applesauce").send_to(network.host("cherrypie").address()); // 从 applesauce 发送到 cherrypie
        dgram_sent.header.ttl--;                                                                   // 减少 TTL
        dgram_sent.header.compute_checksum();                                                      // 计算校验和
        network.host("cherrypie").expect(dgram_sent);                                              // 期望 cherrypie 接收到数据报
        network.simulate();                                                                        // 模拟网络
    }

    cout << green << "\n\nTesting traffic between two ordinary hosts (cherrypie to applesauce)..." << normal
         << "\n\n";
    {
        auto dgram_sent = network.host("cherrypie").send_to(network.host("applesauce").address()); // 从 cherrypie 发送到 applesauce
        dgram_sent.header.ttl--;                                                                   // 减少 TTL
        dgram_sent.header.compute_checksum();                                                      // 计算校验和
        network.host("applesauce").expect(dgram_sent);                                             // 期望 applesauce 接收到数据报
        network.simulate();                                                                        // 模拟网络
    }

    cout << green << "\n\nSuccess! Testing applesauce sending to the Internet." << normal << "\n\n";
    {
        auto dgram_sent = network.host("applesauce").send_to(Address{"1.2.3.4"}); // 从 applesauce 发送到互联网地址
        dgram_sent.header.ttl--;                                                  // 减少 TTL
        dgram_sent.header.compute_checksum();                                     // 计算校验和
        network.host("default_router").expect(dgram_sent);                        // 期望 default_router 接收到数据报
        network.simulate();                                                       // 模拟网络
    }

    cout << green << "\n\nSuccess! Testing sending to the HS network and Internet." << normal << "\n\n";
    {
        auto dgram_sent = network.host("applesauce").send_to(Address{"143.195.131.17"}); // 发送到 HS 网络
        dgram_sent.header.ttl--;                                                         // 减少 TTL
        dgram_sent.header.compute_checksum();                                            // 计算校验和
        network.host("hs_router").expect(dgram_sent);                                    // 期望 hs_router 接收到数据报
        network.simulate();                                                              // 模拟网络

        dgram_sent = network.host("cherrypie").send_to(Address{"143.195.193.52"}); // 发送到 HS 网络
        dgram_sent.header.ttl--;                                                   // 减少 TTL
        dgram_sent.header.compute_checksum();                                      // 计算校验和
        network.host("hs_router").expect(dgram_sent);                              // 期望 hs_router 接收到数据报
        network.simulate();                                                        // 模拟网络

        dgram_sent = network.host("cherrypie").send_to(Address{"143.195.223.255"}); // 发送到 HS 网络
        dgram_sent.header.ttl--;                                                    // 减少 TTL
        dgram_sent.header.compute_checksum();                                       // 计算校验和
        network.host("hs_router").expect(dgram_sent);                               // 期望 hs_router 接收到数据报
        network.simulate();                                                         // 模拟网络

        dgram_sent = network.host("cherrypie").send_to(Address{"143.195.224.0"}); // 发送到互联网
        dgram_sent.header.ttl--;                                                  // 减少 TTL
        dgram_sent.header.compute_checksum();                                     // 计算校验和
        network.host("default_router").expect(dgram_sent);                        // 期望 default_router 接收到数据报
        network.simulate();                                                       // 模拟网络
    }

    cout << green << "\n\nSuccess! Testing two hosts on the same network (dm42 to dm43)..." << normal << "\n\n";
    {
        auto dgram_sent = network.host("dm42").send_to(network.host("dm43").address()); // 从 dm42 发送到 dm43
        dgram_sent.header.ttl--;                                                        // 减少 TTL
        dgram_sent.header.compute_checksum();                                           // 计算校验和
        network.host("dm43").expect(dgram_sent);                                        // 期望 dm43 接收到数据报
        network.simulate();                                                             // 模拟网络
    }

    cout << green << "\n\nSuccess! Testing TTL expiration..." << normal << "\n\n";
    {
        auto dgram_sent = network.host("applesauce").send_to(Address{"1.2.3.4"}, 1); // 发送 TTL 为 1 的数据报
        network.simulate();                                                          // 模拟网络

        dgram_sent = network.host("applesauce").send_to(Address{"1.2.3.4"}, 0); // 发送 TTL 为 0 的数据报
        network.simulate();                                                     // 模拟网络
    }
    cout << "\n\n\033[32;1mCongratulations! All datagrams were routed successfully.\033[m\n"; // 输出成功信息
}

int main()
{
    try
    {
        network_simulator(); // 调用网络模拟器
    }
    catch (const exception &e) // 捕获异常
    {
        cerr << "\n\n\n";
        cerr << "\033[31;1mError: " << e.what() << "\033[m\n"; // 输出错误信息
        return EXIT_FAILURE;                                   // 返回失败状态
    }
    return EXIT_SUCCESS; // 返回成功状态
}
