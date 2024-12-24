#ifndef ROUTER_H
#define ROUTER_H

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include "address.h"
#include "exception.h"
#include "network_interface.h"

// Router 类用于模拟一个简单的路由器
class Router
{
public:
    // 添加一个网络接口到路由器
    // 参数：interface - 共享指针，指向要添加的网络接口
    // 返回值：新添加的接口在接口列表中的索引
    size_t add_interface(std::shared_ptr<NetworkInterface> interface)
    {
        // 将接口添加到接口列表中，并确保接口不为空
        _interfaces.push_back(notnull("add_interface", std::move(interface)));
        return _interfaces.size() - 1; // 返回新接口的索引
    }

    // 获取指定索引的网络接口
    // 参数：idx - 接口的索引
    // 返回值：共享指针，指向指定的网络接口
    std::shared_ptr<NetworkInterface> interface(const size_t idx) { return _interfaces.at(idx); }

    // 添加一条路由到路由表
    // 参数：
    //   route_prefix - 路由前缀（网络地址）
    //   prefix_length - 前缀长度（子网掩码长度）
    //   next_hop - 下一跳地址（可选）
    //   interface_num - 出接口的索引
    void add_route(uint32_t route_prefix, uint8_t prefix_length, std::optional<Address> next_hop, size_t interface_num);

    // 执行路由操作，处理路由器的转发逻辑
    void route();

private:
    // 存储路由器的网络接口列表
    std::vector<std::shared_ptr<NetworkInterface>> _interfaces{};

    // 定义路由表的条目类型
    // 包含出接口的索引和可选的下一跳地址
    using route_entry = std::pair<size_t, std::optional<Address>>;

    // 路由表，使用数组存储32个unordered_map，每个map对应一个前缀长度
    // 每个map的键是路由前缀，值是路由条目
    std::array<std::unordered_map<uint32_t, route_entry>, 32> routing_table_{};

    // 匹配给定地址的路由条目
    // 参数：addr - 要匹配的IP地址
    // 返回值：匹配的路由条目（如果有），否则返回std::nullopt
    std::optional<route_entry> match(uint32_t addr) const;
};

#endif
