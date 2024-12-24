#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

#include <queue>
#include <compare>
#include <optional>
#include <unordered_map>
#include "address.h"
#include "arp_message.h"
#include "ethernet_frame.h"
#include "ipv4_datagram.h"

// NetworkInterface 类将 IP 层（网络层）与以太网层（链路层）连接起来。
// 它是 TCP/IP 协议栈中的一个关键组件，负责将 IP 数据报转换为以太网帧，反之亦然。
// 该类也用于路由器中，路由器具有多个网络接口，用于在不同网络之间路由数据报。

class NetworkInterface
{
public:
    // 表示物理输出端口的抽象类，用于发送以太网帧。
    class OutputPort
    {
    public:
        // 通过输出端口发送以太网帧。
        virtual void transmit(const NetworkInterface &sender, const EthernetFrame &frame) = 0;
        virtual ~OutputPort() = default;
    };

    // 构造函数，用于初始化网络接口，包括名称、输出端口、以太网地址和 IP 地址。
    NetworkInterface(std::string_view name,
                     std::shared_ptr<OutputPort> port,
                     const EthernetAddress &ethernet_address,
                     const Address &ip_address);

    // 发送封装在以太网帧中的 IP 数据报。如果不知道下一跳的以太网地址，
    // 则使用 ARP 进行解析。通过调用输出端口的 `transmit()` 方法发送帧。
    void send_datagram(const InternetDatagram &dgram, const Address &next_hop);

    // 接收以太网帧并进行处理。如果帧包含 IPv4 数据报，则将其添加到
    // `datagrams_received_` 队列中。如果包含 ARP 请求或回复，则相应地更新 ARP 表。
    void recv_frame(const EthernetFrame &frame);

    // 定期调用以处理基于时间的事件，例如过期的 ARP 表项。
    void tick(size_t ms_since_last_tick);

    // 访问器方法，用于获取接口的名称、输出端口和接收到的数据报。
    const std::string &name() const { return name_; }
    const OutputPort &output() const { return *port_; }
    OutputPort &output() { return *port_; }
    std::queue<InternetDatagram> &datagrams_received() { return datagrams_received_; }

private:
    // 辅助类，用于存储以太网地址映射及其过期计时器。
    class AddrMapping
    {
        EthernetAddress ether_addr_; // 以太网地址。
        size_t timer_;               // 计时器。

    public:
        explicit AddrMapping(EthernetAddress ether_addr) : ether_addr_{std::move(ether_addr)}, timer_{} {}
        EthernetAddress get_ether() const noexcept { return ether_addr_; }
        // 提供一种方便的方法来更新计时器，在 `tick()` 方法中使用。
        AddrMapping &operator+=(const size_t ms_time_passed) noexcept { return tick(ms_time_passed); }
        // 将计时器与截止时间进行比较，以确定映射是否已过期。
        auto operator<=>(const size_t deadline) const { return timer_ <=> deadline; }
        // 按指定的时间量更新计时器。
        AddrMapping &tick(const size_t ms_time_passed) noexcept;
    };

    std::string name_; // 网络接口名称。

    // 用于发送以太网帧的输出端口的共享指针。
    std::shared_ptr<OutputPort> port_;
    // 辅助函数，通过输出端口发送以太网帧。
    void transmit(const EthernetFrame &frame) const { port_->transmit(*this, frame); }

    EthernetAddress ethernet_address_; // 接口的以太网地址。
    Address ip_address_;               // 接口的 IP 地址。

    std::queue<InternetDatagram> datagrams_received_{}; // 接收到的 IP 数据报队列。

    const uint32_t rto_map_ = 30000; // ARP 表项的超时时间（30 秒）。
    const uint32_t rto_arp_ = 5000;  // ARP 请求的超时时间（5 秒）。

    // ARP 表，将 IP 地址映射到以太网地址，并带有过期计时器。
    std::unordered_map<uint32_t, AddrMapping> arp_addr_table_{};

    // 跟踪为特定 IP 地址发送的 ARP 请求，带有计时器以防止频繁重发。
    std::unordered_map<uint32_t, uint32_t> arp_requests_{};

    // 存储等待 ARP 解析的 IP 数据报，以目标 IP 地址为键。
    std::unordered_multimap<uint32_t, InternetDatagram> datagrams_waiting_{};

    // 辅助函数，用于创建具有指定参数的 ARP 消息。
    ARPMessage make_arp_mesg(const uint16_t option, const uint32_t target_ip, std::optional<EthernetAddress> target_ether = std::nullopt) const;

    // 辅助函数，用于创建具有指定协议和有效负载的以太网帧。
    EthernetFrame make_frame(const uint16_t protocol, std::vector<std::string> payload, std::optional<EthernetAddress> dst = std::nullopt) const;
};

#endif
