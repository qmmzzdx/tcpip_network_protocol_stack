#ifndef NETWORK_INTERFACE_TEST_HARNESS_H
#define NETWORK_INTERFACE_TEST_HARNESS_H

#include <compare>             // 引入比较库，提供三路比较功能
#include <optional>            // 引入可选类型库，提供 std::optional
#include <utility>             // 引入实用工具库，提供 std::move 等功能
#include "arp_message.h"       // 引入 ARP 消息的定义
#include "common.h"            // 引入公共定义和工具
#include "network_interface.h" // 引入网络接口的定义

class FramesOut : public NetworkInterface::OutputPort
{
public:
    std::queue<EthernetFrame> frames{};                                                                            // 存储发送的以太网帧的队列
    void transmit(const NetworkInterface &n [[maybe_unused]], const EthernetFrame &x) override { frames.push(x); } // 重写 transmit 方法，将帧推入队列
};

using Output = std::shared_ptr<FramesOut>;                      // 定义 Output 为 FramesOut 的智能指针类型
using InterfaceAndOutput = std::pair<NetworkInterface, Output>; // 定义 InterfaceAndOutput 为网络接口和输出的配对

class NetworkInterfaceTestHarness : public TestHarness<InterfaceAndOutput>
{
public:
    NetworkInterfaceTestHarness(std::string test_name,
                                const EthernetAddress &ethernet_address,
                                const Address &ip_address)
        : TestHarness(move(test_name), "eth=" + to_string(ethernet_address) + ", ip=" + ip_address.ip(), [&]
                      {
                          const Output output{std::make_shared<FramesOut>()};                         // 创建 FramesOut 的共享指针
                          const NetworkInterface iface{"test", output, ethernet_address, ip_address}; // 创建网络接口实例
                          return InterfaceAndOutput{iface, output};                                   // 返回网络接口和输出的配对
                      }())
    {
    }
};

inline std::string summary(const EthernetFrame &frame);

struct SendDatagram : public Action<InterfaceAndOutput>
{
    InternetDatagram dgram; // 要发送的数据报
    Address next_hop;       // 下一跳地址

    std::string description() const override
    {
        return "request to send datagram (to next hop " + next_hop.ip() + "): " + dgram.header.to_string();
    }

    void execute(InterfaceAndOutput &interface) const override { interface.first.send_datagram(dgram, next_hop); }

    SendDatagram(InternetDatagram d, Address n) : dgram(std::move(d)), next_hop(n) {}
};

inline std::string concat(const std::vector<std::string> &buffers)
{
    return std::accumulate(buffers.begin(), buffers.end(), std::string{}); // 将字符串向量连接成一个字符串
}

template <class T>
bool equal(const T &t1, const T &t2)
{
    const std::vector<std::string> t1s = serialize(t1); // 序列化第一个对象
    const std::vector<std::string> t2s = serialize(t2); // 序列化第二个对象

    return concat(t1s) == concat(t2s); // 比较两个序列化后的字符串
}

struct ReceiveFrame : public Action<InterfaceAndOutput>
{
    EthernetFrame frame;                      // 接收到的以太网帧
    std::optional<InternetDatagram> expected; // 预期的数据报（可选）

    std::string description() const override { return "frame arrives (" + summary(frame) + ")"; }
    void execute(InterfaceAndOutput &interface) const override
    {
        interface.first.recv_frame(frame); // 接收以太网帧

        auto &inbound = interface.first.datagrams_received(); // 获取接收到的数据报

        if (not expected.has_value())
        {
            if (inbound.empty())
            {
                return; // 如果没有预期的数据报且接收队列为空，直接返回
            }
            throw ExpectationViolation(
                "an arriving Ethernet frame was passed up the stack as an Internet datagram, but was not expected to be "
                "(did destination address match our interface?)");
        }

        if (inbound.empty())
        {
            throw ExpectationViolation(
                "an arriving Ethernet frame was expected to be passed up the stack as an Internet datagram, but wasn't");
        }

        if (not equal(inbound.front(), expected.value()))
        {
            throw ExpectationViolation(
                std::string("NetworkInterface::recv_frame() produced a different Internet datagram than was expected: ") + "actual={" + inbound.front().header.to_string() + "}");
        }

        inbound.pop(); // 移除已处理的数据报
    }

    ReceiveFrame(EthernetFrame f, std::optional<InternetDatagram> e)
        : frame(std::move(f)), expected(std::move(e))
    {
    }
};

struct ExpectFrame : public Expectation<InterfaceAndOutput>
{
    EthernetFrame expected; // 预期的以太网帧

    std::string description() const override { return "frame transmitted (" + summary(expected) + ")"; }
    void execute(InterfaceAndOutput &interface) const override
    {
        if (interface.second->frames.empty())
        {
            throw ExpectationViolation("NetworkInterface was expected to send an Ethernet frame, but did not");
        }

        const EthernetFrame frame = std::move(interface.second->frames.front()); // 获取发送的帧
        interface.second->frames.pop();                                          // 移除已处理的帧

        if (not equal(frame, expected))
        {
            throw ExpectationViolation("NetworkInterface sent a different Ethernet frame than was expected: actual={" + summary(frame) + "}");
        }
    }

    explicit ExpectFrame(EthernetFrame e) : expected(std::move(e)) {}
};

struct ExpectNoFrame : public Expectation<InterfaceAndOutput>
{
    std::string description() const override { return "no frame transmitted"; }
    void execute(InterfaceAndOutput &interface) const override
    {
        if (not interface.second->frames.empty())
        {
            throw ExpectationViolation("NetworkInterface sent an Ethernet frame although none was expected");
        }
    }
};

struct Tick : public Action<InterfaceAndOutput>
{
    size_t _ms; // 模拟的时间（毫秒）

    std::string description() const override { return to_string(_ms) + " ms pass"; }
    void execute(InterfaceAndOutput &interface) const override { interface.first.tick(_ms); } // 执行时间推进

    explicit Tick(const size_t ms) : _ms(ms) {}
};

inline std::string summary(const EthernetFrame &frame)
{
    std::string out = frame.header.to_string() + " payload: "; // 生成帧头的字符串表示
    switch (frame.header.type)
    {
        case EthernetHeader::TYPE_IPv4:
        {
            InternetDatagram dgram;
            if (parse(dgram, frame.payload)) // 解析 IPv4 数据报
            {
                out.append(dgram.header.to_string() + " payload=\"" + Printer::prettify(concat(dgram.payload)) + "\"");
            }
            else
            {
                out.append("bad IPv4 datagram");
            }
            break;
        }
        case EthernetHeader::TYPE_ARP:
        {
            ARPMessage arp;
            if (parse(arp, frame.payload)) // 解析 ARP 消息
            {
                out.append(arp.to_string());
            }
            else
            {
                out.append("bad ARP message");
            }
            break;
        }
        default:
        {
            out.append("unknown frame type"); // 未知帧类型
            break;
        }
    }
    return out; // 返回帧的摘要信息
}

#endif
