#include <iostream>
#include <cstddef>
#include <bit>
#include <optional>
#include <vector>
#include <unordered_map>
#include "router.h"

// 方法：向路由器的路由表中添加一条路由
// 参数：
// - route_prefix: 路由前缀的数值表示
// - prefix_length: 前缀长度
// - next_hop: 下一跳地址（可选）
// - interface_num: 接口编号
void Router::add_route(uint32_t route_prefix, uint8_t prefix_length, std::optional<Address> next_hop, size_t interface_num)
{
    // 输出调试信息，显示正在添加的路由
    std::cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/";
    std::cerr << static_cast<int>(prefix_length) << " => " << (next_hop ? next_hop->ip() : "(direct)");
    std::cerr << " on interface " << interface_num << "\n";

    // 通过旋转路由前缀计算掩码前缀，并将路由存储到路由表中
    routing_table_[prefix_length][std::rotr(route_prefix, 32 - prefix_length)] = {interface_num, next_hop};
}

// 方法：处理并路由所有接口接收到的数据报
void Router::route()
{
    // 遍历每个网络接口
    for (const auto &interface : _interfaces)
    {
        // 获取该接口接收到的数据报队列
        auto &dgrams_queue = interface->datagrams_received();

        // 处理队列中的每个数据报
        while (!dgrams_queue.empty())
        {
            // 从队列中移出数据报
            InternetDatagram datagram = std::move(dgrams_queue.front());
            dgrams_queue.pop();

            // 如果TTL过低，则丢弃数据报
            if (datagram.header.ttl <= 1)
            {
                continue;
            }

            // 减少TTL并重新计算校验和
            datagram.header.ttl--;
            datagram.header.compute_checksum();

            // 查找与数据报目的地址匹配的最佳路由
            std::optional<Router::route_entry> route = match(datagram.header.dst);

            // 如果找到匹配的路由，则转发数据报
            if (route.has_value())
            {
                // 从路由中提取接口编号和下一跳地址
                size_t interface_num = route.value().first;
                std::optional<Address> next_hop = route.value().second;

                // 确定数据报的目的地址
                Address destination = next_hop.value_or(Address::from_ipv4_numeric(datagram.header.dst));

                // 在指定接口上发送数据报
                _interfaces[interface_num]->send_datagram(datagram, destination);
            }
        }
    }
}

// 方法：查找给定目的地址的最佳匹配路由
// 参数：
// - addr: 目的地址的数值表示
// 返回值：
// - 匹配的路由条目（如果找到），否则返回std::nullopt
std::optional<Router::route_entry> Router::match(uint32_t addr) const
{
    for (int prefix_length = 31; prefix_length >= 0; --prefix_length)
    {
        if (auto it = routing_table_[prefix_length].find(addr >>= 1); it != routing_table_[prefix_length].end())
        {
            return it->second; // 返回匹配的路由条目
        }
    }
    return std::nullopt; // 如果没有找到匹配的路由条目
}
