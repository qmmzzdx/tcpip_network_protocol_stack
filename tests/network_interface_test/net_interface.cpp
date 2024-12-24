#include <iostream>                         // 引入输入输出流库
#include <cstdlib>                          // 引入 C 标准库，提供 EXIT_SUCCESS 和 EXIT_FAILURE
#include <random>                           // 引入随机数生成库
#include "arp_message.h"                    // 引入 ARP 消息的定义
#include "ethernet_header.h"                // 引入以太网头部的定义
#include "ipv4_datagram.h"                  // 引入 IPv4 数据报的定义
#include "network_interface_test_harness.h" // 引入网络接口测试工具的定义
using namespace std;

EthernetAddress random_private_ethernet_address()
{
    EthernetAddress addr; // 创建一个以太网地址对象
    for (auto &byte : addr)
    {
        byte = random_device()(); // 使用随机设备生成随机字节
    }
    addr.at(0) |= 0x02; // 设置地址的第一个字节的最低两位为 "10"，标记为私有以太网地址
    addr.at(0) &= 0xfe; // 清除最低位，确保地址是单播地址

    return addr; // 返回生成的私有以太网地址
}

InternetDatagram make_datagram(const string &src_ip, const string &dst_ip) // NOLINT(*-swappable-*)
{
    InternetDatagram dgram;                                                                         // 创建一个 InternetDatagram 对象
    dgram.header.src = Address(src_ip, 0).ipv4_numeric();                                           // 设置源 IP 地址
    dgram.header.dst = Address(dst_ip, 0).ipv4_numeric();                                           // 设置目标 IP 地址
    dgram.payload.emplace_back("hello");                                                            // 添加负载数据
    dgram.header.len = static_cast<uint64_t>(dgram.header.hlen) * 4 + dgram.payload.front().size(); // 计算数据报长度
    dgram.header.compute_checksum();                                                                // 计算校验和
    return dgram;                                                                                   // 返回构建好的数据报
}

ARPMessage make_arp(const uint16_t opcode,
                    const EthernetAddress sender_ethernet_address,
                    const string &sender_ip_address,
                    const EthernetAddress target_ethernet_address,
                    const string &target_ip_address)
{
    ARPMessage arp;                                                       // 创建一个 ARPMessage 对象
    arp.opcode = opcode;                                                  // 设置操作码（请求或回复）
    arp.sender_ethernet_address = sender_ethernet_address;                // 设置发送者以太网地址
    arp.sender_ip_address = Address(sender_ip_address, 0).ipv4_numeric(); // 设置发送者 IP 地址
    arp.target_ethernet_address = target_ethernet_address;                // 设置目标以太网地址
    arp.target_ip_address = Address(target_ip_address, 0).ipv4_numeric(); // 设置目标 IP 地址
    return arp;                                                           // 返回构建好的 ARP 消息
}

EthernetFrame make_frame(const EthernetAddress &src,
                         const EthernetAddress &dst,
                         const uint16_t type,
                         vector<string> payload)
{
    EthernetFrame frame;                // 创建一个 EthernetFrame 对象
    frame.header.src = src;             // 设置源以太网地址
    frame.header.dst = dst;             // 设置目标以太网地址
    frame.header.type = type;           // 设置以太网帧类型
    frame.payload = std::move(payload); // 移动负载数据
    return frame;                       // 返回构建好的以太网帧
}

int main()
{
    try
    {
        {
            const EthernetAddress local_eth = random_private_ethernet_address();                        // 生成本地以太网地址
            NetworkInterfaceTestHarness test{"typical ARP workflow", local_eth, Address("4.3.2.1", 0)}; // 创建测试工具

            const auto datagram = make_datagram("5.6.7.8", "13.12.11.10");   // 创建数据报
            test.execute(SendDatagram{datagram, Address("192.168.0.1", 0)}); // 发送数据报

            // 发送数据报应导致 ARP 请求
            test.execute(ExpectFrame{make_frame(
                local_eth,
                ETHERNET_BROADCAST,
                EthernetHeader::TYPE_ARP,
                serialize(make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "4.3.2.1", {}, "192.168.0.1")))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            const EthernetAddress target_eth = random_private_ethernet_address(); // 生成目标以太网地址

            test.execute(Tick{800});       // 模拟时间流逝
            test.execute(ExpectNoFrame{}); // 期望没有帧

            // ARP 回复应导致排队的数据报被发送
            test.execute(ReceiveFrame{
                make_frame(
                    target_eth,
                    local_eth,
                    EthernetHeader::TYPE_ARP, // NOLINTNEXTLINE(*-suspicious-*)
                    serialize(make_arp(ARPMessage::OPCODE_REPLY, target_eth, "192.168.0.1", local_eth, "4.3.2.1"))),
                {}});

            test.execute(
                ExpectFrame{make_frame(local_eth, target_eth, EthernetHeader::TYPE_IPv4, serialize(datagram))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            // 任何发往我们以太网地址的 IP 回复应被传递到上层
            const auto reply_datagram = make_datagram("13.12.11.10", "5.6.7.8");
            test.execute(
                ReceiveFrame(make_frame(target_eth, local_eth, EthernetHeader::TYPE_IPv4, serialize(reply_datagram)),
                             reply_datagram));
            test.execute(ExpectNoFrame{}); // 期望没有帧

            // 发往其他以太网地址的帧（不是我们的）应被忽略
            const EthernetAddress another_eth = {1, 1, 1, 1, 1, 1};
            test.execute(ReceiveFrame(
                make_frame(target_eth, another_eth, EthernetHeader::TYPE_IPv4, serialize(reply_datagram)), {}));
        }
        {
            const EthernetAddress local_eth = random_private_ethernet_address();                        // 生成本地以太网地址
            const EthernetAddress remote_eth = random_private_ethernet_address();                       // 生成远程以太网地址
            NetworkInterfaceTestHarness test{"reply to ARP request", local_eth, Address("5.5.5.5", 0)}; // 创建测试工具
            test.execute(ReceiveFrame{
                make_frame(remote_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           serialize(make_arp(ARPMessage::OPCODE_REQUEST, remote_eth, "10.0.1.1", {}, "7.7.7.7"))),
                {}});
            test.execute(ExpectNoFrame{}); // 期望没有帧
            test.execute(ReceiveFrame{
                make_frame(remote_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           serialize(make_arp(ARPMessage::OPCODE_REQUEST, remote_eth, "10.0.1.1", {}, "5.5.5.5"))),
                {}});
            test.execute(ExpectFrame{make_frame(
                local_eth,
                remote_eth,
                EthernetHeader::TYPE_ARP,
                serialize(make_arp(ARPMessage::OPCODE_REPLY, local_eth, "5.5.5.5", remote_eth, "10.0.1.1")))});
            test.execute(ExpectNoFrame{}); // 期望没有帧
        }
        {
            const EthernetAddress local_eth = random_private_ethernet_address();                          // 生成本地以太网地址
            const EthernetAddress remote_eth = random_private_ethernet_address();                         // 生成远程以太网地址
            NetworkInterfaceTestHarness test{"learn from ARP request", local_eth, Address("5.5.5.5", 0)}; // 创建测试工具
            test.execute(ReceiveFrame{
                make_frame(remote_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           serialize(make_arp(ARPMessage::OPCODE_REQUEST, remote_eth, "10.0.1.1", {}, "5.5.5.5"))),
                {}});
            test.execute(ExpectFrame{make_frame(
                local_eth,
                remote_eth,
                EthernetHeader::TYPE_ARP,
                serialize(make_arp(ARPMessage::OPCODE_REPLY, local_eth, "5.5.5.5", remote_eth, "10.0.1.1")))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            const auto datagram = make_datagram("5.6.7.8", "13.12.11.10"); // 创建数据报
            test.execute(SendDatagram{datagram, Address("10.0.1.1", 0)});  // 发送数据报
            test.execute(
                ExpectFrame{make_frame(local_eth, remote_eth, EthernetHeader::TYPE_IPv4, serialize(datagram))});
            test.execute(ExpectNoFrame{}); // 期望没有帧
        }
        {
            const EthernetAddress local_eth = random_private_ethernet_address();                                      // 生成本地以太网地址
            NetworkInterfaceTestHarness test{"pending mappings last five seconds", local_eth, Address("1.2.3.4", 0)}; // 创建测试工具

            test.execute(SendDatagram{make_datagram("5.6.7.8", "13.12.11.10"), Address("10.0.0.1", 0)}); // 发送数据报
            test.execute(ExpectFrame{
                make_frame(local_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           serialize(make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "1.2.3.4", {}, "10.0.0.1")))});
            test.execute(ExpectNoFrame{});                                                                   // 期望没有帧
            test.execute(Tick{4990});                                                                        // 模拟时间流逝
            test.execute(SendDatagram{make_datagram("17.17.17.17", "18.18.18.18"), Address("10.0.0.1", 0)}); // 发送数据报
            test.execute(ExpectNoFrame{});                                                                   // 期望没有帧
            test.execute(Tick{20});                                                                          // 模拟时间流逝
            // pending mapping 应该现在过期
            test.execute(SendDatagram{make_datagram("42.41.40.39", "13.12.11.10"), Address("10.0.0.1", 0)}); // 发送数据报
            test.execute(ExpectFrame{
                make_frame(local_eth,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           serialize(make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "1.2.3.4", {}, "10.0.0.1")))});
            test.execute(ExpectNoFrame{}); // 期望没有帧
        }
        {
            const EthernetAddress local_eth = random_private_ethernet_address();                                   // 生成本地以太网地址
            NetworkInterfaceTestHarness test{"active mappings last 30 seconds", local_eth, Address("4.3.2.1", 0)}; // 创建测试工具

            const auto datagram = make_datagram("5.6.7.8", "13.12.11.10");  // 创建数据报
            const auto datagram2 = make_datagram("5.6.7.8", "13.12.11.11"); // 创建第二个数据报
            const auto datagram3 = make_datagram("5.6.7.8", "13.12.11.12"); // 创建第三个数据报
            const auto datagram4 = make_datagram("5.6.7.8", "13.12.11.13"); // 创建第四个数据报

            test.execute(SendDatagram{datagram, Address("192.168.0.1", 0)}); // 发送数据报
            test.execute(ExpectFrame{make_frame(
                local_eth,
                ETHERNET_BROADCAST,
                EthernetHeader::TYPE_ARP,
                serialize(make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "4.3.2.1", {}, "192.168.0.1")))});
            const EthernetAddress target_eth = random_private_ethernet_address(); // 生成目标以太网地址
            test.execute(ReceiveFrame{
                make_frame(
                    target_eth,
                    local_eth,
                    EthernetHeader::TYPE_ARP, // NOLINTNEXTLINE(*-suspicious-*)
                    serialize(make_arp(ARPMessage::OPCODE_REPLY, target_eth, "192.168.0.1", local_eth, "4.3.2.1"))),
                {}});

            test.execute(
                ExpectFrame{make_frame(local_eth, target_eth, EthernetHeader::TYPE_IPv4, serialize(datagram))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            // 尝试在 10 秒后发送数据报 -- 不应生成 ARP
            test.execute(Tick{10000});                                        // 模拟时间流逝
            test.execute(SendDatagram{datagram2, Address("192.168.0.1", 0)}); // 发送第二个数据报
            test.execute(
                ExpectFrame{make_frame(local_eth, target_eth, EthernetHeader::TYPE_IPv4, serialize(datagram2))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            // 再过 10 秒 -- 不应生成 ARP
            test.execute(Tick{10000});                                        // 模拟时间流逝
            test.execute(SendDatagram{datagram3, Address("192.168.0.1", 0)}); // 发送第三个数据报
            test.execute(
                ExpectFrame{make_frame(local_eth, target_eth, EthernetHeader::TYPE_IPv4, serialize(datagram3))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            // 再过 11 秒，需要重新 ARP
            test.execute(Tick{11000});                                        // 模拟时间流逝
            test.execute(SendDatagram{datagram4, Address("192.168.0.1", 0)}); // 发送第四个数据报
            test.execute(ExpectFrame{make_frame(
                local_eth,
                ETHERNET_BROADCAST,
                EthernetHeader::TYPE_ARP,
                serialize(make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "4.3.2.1", {}, "192.168.0.1")))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            // 再次 ARP 回复
            const EthernetAddress new_target_eth = random_private_ethernet_address(); // 生成新的目标以太网地址
            test.execute(ReceiveFrame{
                make_frame(
                    new_target_eth,
                    local_eth,
                    EthernetHeader::TYPE_ARP,
                    serialize(make_arp(ARPMessage::OPCODE_REPLY, new_target_eth, "192.168.0.1", local_eth, "4.3.2.1"))),
                {}});
            test.execute(ExpectFrame{
                make_frame(local_eth, new_target_eth, EthernetHeader::TYPE_IPv4, serialize(datagram4))});
            test.execute(ExpectNoFrame{}); // 期望没有帧
        }
        {
            const EthernetAddress local_eth = random_private_ethernet_address();   // 生成本地以太网地址
            const EthernetAddress remote_eth1 = random_private_ethernet_address(); // 生成第一个远程以太网地址
            const EthernetAddress remote_eth2 = random_private_ethernet_address(); // 生成第二个远程以太网地址
            NetworkInterfaceTestHarness test{
                "different ARP mappings are independent", local_eth, Address("10.0.0.1", 0)}; // 创建测试工具

            // 第一个 ARP 映射
            test.execute(ReceiveFrame{
                make_frame(remote_eth1,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           serialize(make_arp(ARPMessage::OPCODE_REQUEST, remote_eth1, "10.0.0.5", {}, "10.0.0.1"))),
                {}});
            test.execute(ExpectFrame{make_frame(
                local_eth,
                remote_eth1,
                EthernetHeader::TYPE_ARP,
                serialize(make_arp(ARPMessage::OPCODE_REPLY, local_eth, "10.0.0.1", remote_eth1, "10.0.0.5")))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            test.execute(Tick{15000}); // 模拟时间流逝

            // 第二个 ARP 映射
            test.execute(ReceiveFrame{
                make_frame(remote_eth2,
                           ETHERNET_BROADCAST,
                           EthernetHeader::TYPE_ARP,
                           serialize(make_arp(ARPMessage::OPCODE_REQUEST, remote_eth2, "10.0.0.19", {}, "10.0.0.1"))),
                {}});
            test.execute(ExpectFrame{make_frame(
                local_eth,
                remote_eth2,
                EthernetHeader::TYPE_ARP,
                serialize(make_arp(ARPMessage::OPCODE_REPLY, local_eth, "10.0.0.1", remote_eth2, "10.0.0.19")))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            test.execute(Tick{10000}); // 模拟时间流逝

            // 发送到第一个目标的数据报
            const auto datagram = make_datagram("5.6.7.8", "13.12.11.10"); // 创建数据报
            test.execute(SendDatagram{datagram, Address("10.0.0.5", 0)});  // 发送数据报

            // 发送到第二个目标的数据报
            const auto datagram2 = make_datagram("100.99.98.97", "4.10.4.10"); // 创建第二个数据报
            test.execute(SendDatagram{datagram2, Address("10.0.0.19", 0)});    // 发送数据报

            test.execute(
                ExpectFrame{make_frame(local_eth, remote_eth1, EthernetHeader::TYPE_IPv4, serialize(datagram))});
            test.execute(
                ExpectFrame{make_frame(local_eth, remote_eth2, EthernetHeader::TYPE_IPv4, serialize(datagram2))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            test.execute(Tick{5010}); // 模拟时间流逝

            // 发送到第二个目标的数据报（映射仍然有效）
            const auto datagram3 = make_datagram("150.140.130.120", "144.144.144.144"); // 创建第三个数据报
            test.execute(SendDatagram{datagram3, Address("10.0.0.19", 0)});             // 发送数据报
            test.execute(
                ExpectFrame{make_frame(local_eth, remote_eth2, EthernetHeader::TYPE_IPv4, serialize(datagram3))});
            test.execute(ExpectNoFrame{}); // 期望没有帧

            // 发送到第二个目标的数据报（映射已过期）
            const auto datagram4 = make_datagram("244.244.244.244", "3.3.3.3"); // 创建第四个数据报
            test.execute(SendDatagram{datagram4, Address("10.0.0.5", 0)});      // 发送数据报
            test.execute(ExpectFrame{make_frame(
                local_eth,
                ETHERNET_BROADCAST,
                EthernetHeader::TYPE_ARP,
                serialize(make_arp(ARPMessage::OPCODE_REQUEST, local_eth, "10.0.0.1", {}, "10.0.0.5")))});
            test.execute(ExpectNoFrame{}); // 期望没有帧
        }
    }
    catch (const exception &e)
    {
        cerr << e.what() << endl; // 捕获异常并输出错误信息
        return EXIT_FAILURE;      // 返回失败状态
    }
    return EXIT_SUCCESS; // 返回成功状态
}
