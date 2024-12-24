#include <iostream>
#include "exception.h"
#include "network_interface.h"

// 构造函数：初始化网络接口
NetworkInterface::NetworkInterface(std::string_view name,
                                   std::shared_ptr<OutputPort> port,
                                   const EthernetAddress &ethernet_address,
                                   const Address &ip_address)
    : name_(name), port_(notnull("OutputPort", std::move(port))), ethernet_address_(ethernet_address), ip_address_(ip_address)
{
    // 输出调试信息，显示以太网地址和 IP 地址
    std::cerr << "DEBUG: Network interface has Ethernet address " << to_string(ethernet_address)
              << " and IP address " << ip_address.ip() << "\n";
}

// 发送 IPv4 数据报
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop)
{
    const uint32_t target_ip = next_hop.ipv4_numeric();
    if (auto iter = arp_addr_table_.find(target_ip); iter != arp_addr_table_.end())
    {
        // 如果找到目标 IP 的以太网地址，直接发送数据报
        transmit(make_frame(EthernetHeader::TYPE_IPv4, serialize(dgram), iter->second.get_ether()));
        return;
    }
    // 如果找不到目标 IP 的以太网地址，将数据报加入等待队列
    datagrams_waiting_.emplace(target_ip, dgram);

    // 如果五秒内没有发送过相同的地址解析请求，则发送 ARP 请求
    if (arp_requests_.find(target_ip) == arp_requests_.end())
    {
        transmit(make_frame(EthernetHeader::TYPE_ARP,
                            serialize(make_arp_mesg(ARPMessage::OPCODE_REQUEST, target_ip))));
        arp_requests_.emplace(target_ip, 0);
    }
}

// 接收以太网帧并做出适当响应
void NetworkInterface::recv_frame(const EthernetFrame &frame)
{
    // 丢弃目的地址既不是广播地址也不是本接口的数据帧
    if (frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_)
    {
        return;
    }

    if (frame.header.type == EthernetHeader::TYPE_IPv4)
    {
        InternetDatagram ip_dgram;
        if (parse(ip_dgram, frame.payload))
        {
            // 解析成功后将数据报推入 datagrams_received_ 队列
            datagrams_received_.emplace(std::move(ip_dgram));
        }
    }
    else if (frame.header.type == EthernetHeader::TYPE_ARP)
    {
        ARPMessage arp_msg;
        if (parse(arp_msg, frame.payload))
        {
            // 更新 ARP 地址映射表
            arp_addr_table_.insert_or_assign(arp_msg.sender_ip_address, AddrMapping(arp_msg.sender_ethernet_address));

            if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST &&
                arp_msg.target_ip_address == ip_address_.ipv4_numeric())
            {
                // 如果目标 IP 地址与本接口的 IP 地址一致，发送 ARP 响应
                transmit(make_frame(EthernetHeader::TYPE_ARP,
                                    serialize(make_arp_mesg(ARPMessage::OPCODE_REPLY,
                                                            arp_msg.sender_ip_address,
                                                            arp_msg.sender_ethernet_address)),
                                    arp_msg.sender_ethernet_address));
            }
            else if (arp_msg.opcode == ARPMessage::OPCODE_REPLY)
            {
                // 遍历队列发出等待的 IP 数据报
                auto range = datagrams_waiting_.equal_range(arp_msg.sender_ip_address);
                for (auto it = range.first; it != range.second; ++it)
                {
                    transmit(make_frame(EthernetHeader::TYPE_IPv4, serialize(it->second), arp_msg.sender_ethernet_address));
                }
                datagrams_waiting_.erase(range.first, range.second);
            }
        }
    }
}
// 定期调用以处理时间流逝
void NetworkInterface::tick(size_t ms_since_last_tick)
{
    // 刷新数据表，删除超时项
    auto flush_timer = [ms_since_last_tick](auto &data_table, const uint32_t deadline)
    {
        std::erase_if(data_table, [ms_since_last_tick, deadline](auto &entry)
        {
            entry.second += ms_since_last_tick;
            return entry.second > deadline;
        });
    };
    flush_timer(arp_addr_table_, rto_map_);
    flush_timer(arp_requests_, rto_arp_);
}

// 创建 ARP 消息
ARPMessage NetworkInterface::make_arp_mesg(const uint16_t option, const uint32_t target_ip, std::optional<EthernetAddress> target_ether) const
{
    return {.opcode = option,
            .sender_ethernet_address = ethernet_address_,
            .sender_ip_address = ip_address_.ipv4_numeric(),
            .target_ethernet_address = target_ether.value_or(EthernetAddress{}),
            .target_ip_address = target_ip};
}

// 创建以太网帧
EthernetFrame NetworkInterface::make_frame(const uint16_t protocol, std::vector<std::string> payload, std::optional<EthernetAddress> dst) const
{
    return EthernetFrame{{dst.value_or(ETHERNET_BROADCAST), ethernet_address_, protocol}, std::move(payload)};
}

// 更新 AddrMapping 的计时器
NetworkInterface::AddrMapping &NetworkInterface::AddrMapping::tick(const size_t ms_time_passed) noexcept
{
    timer_ += ms_time_passed;
    return *this;
}
