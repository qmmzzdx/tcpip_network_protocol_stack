#include <cstring>           // 提供 C 字符串处理函数
#include <array>             // 提供 std::array 类
#include <memory>            // 提供智能指针
#include <netdb.h>           // 提供网络数据库操作
#include <stdexcept>         // 提供标准异常类
#include <sys/socket.h>      // 提供 socket 操作
#include <system_error>      // 提供系统错误处理
#include <arpa/inet.h>       // 提供 IP 地址转换函数
#include <linux/if_packet.h> // 提供数据链路层地址结构
#include "address.h"         // 引入 Address 类的头文件
#include "exception.h"       // 引入自定义异常类的头文件

// Raw 类的类型转换运算符，将 Raw 对象转换为 sockaddr*
Address::Raw::operator sockaddr *()
{
    return reinterpret_cast<sockaddr *>(&storage); // NOLINT(*-reinterpret-cast)
}

// Raw 类的类型转换运算符，将 Raw 对象转换为 const sockaddr*
Address::Raw::operator const sockaddr *() const
{
    return reinterpret_cast<const sockaddr *>(&storage); // NOLINT(*-reinterpret-cast)
}

// Address 类的构造函数，使用 sockaddr 指针和大小初始化 Address
Address::Address(const sockaddr *addr, const size_t size) : _size(size)
{
    // 检查传入的地址大小是否有效
    if (size > sizeof(_address.storage))
    {
        throw std::runtime_error("invalid sockaddr size"); // 抛出异常
    }
    std::memcpy(&_address.storage, addr, size); // 复制地址信息
}

// 自定义错误类别，用于处理 getaddrinfo 的错误
class gai_error_category : public std::error_category
{
public:
    // 返回错误类别的名称
    const char *name() const noexcept override { return "gai_error_category"; }

    // 返回错误信息
    std::string message(const int return_value) const noexcept override { return std::string(gai_strerror(return_value)); }
};

// Address 类的构造函数，使用节点、服务和地址信息结构初始化 Address
Address::Address(const std::string &node, const std::string &service, const addrinfo &hints) : _size()
{
    addrinfo *resolved_address = nullptr; // 存储解析后的地址信息

    // 调用 getaddrinfo 解析主机并解析成ipv4或ipv6地址
    const int gai_ret = getaddrinfo(node.c_str(), service.c_str(), &hints, &resolved_address);
    if (gai_ret != 0)
    {
        // 如果解析失败，抛出 tagged_error 异常
        throw tagged_error(gai_error_category(), "getaddrinfo(" + node + ", " + service + ")", gai_ret);
    }

    // 检查解析结果是否为空
    if (resolved_address == nullptr)
    {
        throw std::runtime_error("getaddrinfo returned successfully but with no results");
    }

    // 使用自定义删除器管理 addrinfo 的内存
    auto addrinfo_deleter = [](addrinfo *const x) { freeaddrinfo(x); };
    std::unique_ptr<addrinfo, decltype(addrinfo_deleter)> wrapped_address(resolved_address, std::move(addrinfo_deleter));

    // 使用解析后的地址初始化 Address 对象
    *this = Address(wrapped_address->ai_addr, wrapped_address->ai_addrlen);
}

// 创建 addrinfo 结构的辅助函数
inline addrinfo make_hints(int ai_flags, int ai_family) // NOLINT(*-swappable-parameters)
{
    addrinfo hints{};
    hints.ai_flags = ai_flags, hints.ai_family = ai_family; // ai_flags 用于指定地址解析的选项，ai_family 用于指定地址族
    return hints;
}

// 使用主机名和服务名初始化 Address，默认使用 IPv4
Address::Address(const std::string &hostname, const std::string &service)
    : Address(hostname, service, make_hints(AI_ALL, AF_INET)) {}

// 使用 IP 地址和端口号初始化 Address
Address::Address(const std::string &ip, const uint16_t port)
    : Address(ip, std::to_string(port), make_hints(AI_NUMERICHOST | AI_NUMERICSERV, AF_INET)) {}

// 返回 IP 地址和端口号的配对
std::pair<std::string, uint16_t> Address::ip_port() const
{
    // 检查地址族是否为 IPv4 或 IPv6
    if (_address.storage.ss_family != AF_INET && _address.storage.ss_family != AF_INET6)
    {
        throw std::runtime_error("Address::ip_port() called on non-Internet address");
    }

    std::array<char, NI_MAXHOST> ip{};   // 存储 IP 地址
    std::array<char, NI_MAXSERV> port{}; // 存储端口号

    // 调用 getnameinfo 解析IP和端口信息并生成主机名和服务名
    const int gni_ret = getnameinfo(static_cast<const sockaddr *>(_address), _size, ip.data(), ip.size(), port.data(), port.size(), NI_NUMERICHOST | NI_NUMERICSERV);
    if (gni_ret != 0)
    {
        throw tagged_error(gai_error_category(), "getnameinfo", gni_ret); // 抛出异常
    }
    return {ip.data(), static_cast<uint16_t>(std::stoi(port.data()))}; // 返回 IP 和端口的配对
}

// 将地址转换为字符串表示
std::string Address::to_string() const
{
    // 检查地址族
    if (_address.storage.ss_family == AF_INET || _address.storage.ss_family == AF_INET6)
    {
        const auto ip_and_port = ip_port();                                  // 获取 IP 和端口
        return ip_and_port.first + ":" + std::to_string(ip_and_port.second); // 返回格式化字符串
    }
    return "(non-Internet address)"; // 返回非互联网地址的提示
}

// 返回 IPv4 地址的数值表示
uint32_t Address::ipv4_numeric() const
{
    // 检查地址族和大小
    if (_address.storage.ss_family != AF_INET || _size != sizeof(sockaddr_in))
    {
        throw std::runtime_error("ipv4_numeric called on non-IPV4 address");
    }

    sockaddr_in ipv4_addr{};                           // 创建 sockaddr_in 结构
    std::memcpy(&ipv4_addr, &_address.storage, _size); // 复制地址信息

    return be32toh(ipv4_addr.sin_addr.s_addr); // 返回网络字节序转换后的地址
}

// 从 IPv4 数值地址创建 Address 对象
Address Address::from_ipv4_numeric(const uint32_t ip_address)
{
    sockaddr_in ipv4_addr{};
    ipv4_addr.sin_family = AF_INET;
    ipv4_addr.sin_addr.s_addr = htobe32(ip_address); // 设置地址族和地址

    return {reinterpret_cast<sockaddr *>(&ipv4_addr), sizeof(ipv4_addr)}; // NOLINT(*-reinterpret-cast)
}

// 重载相等运算符
bool Address::operator==(const Address &other) const
{
    return _size != other._size ? false : std::memcmp(&_address, &other._address, _size) == 0; // 比较地址内容
}

// 模板特化，用于获取 sockaddr 类型的地址族
template <typename sockaddr_type>
constexpr int sockaddr_family = -1;

template <>
constexpr int sockaddr_family<sockaddr_in> = AF_INET; // IPv4 地址族

template <>
constexpr int sockaddr_family<sockaddr_in6> = AF_INET6; // IPv6 地址族

template <>
constexpr int sockaddr_family<sockaddr_ll> = AF_PACKET; // 数据链路层地址族

// 将 Address 转换为指定类型的 sockaddr*
template <typename sockaddr_type>
const sockaddr_type *Address::as() const
{
    const sockaddr *raw{_address}; // 获取原始 sockaddr 指针

    // 检查大小和地址族是否匹配
    if (sizeof(sockaddr_type) < size() || raw->sa_family != sockaddr_family<sockaddr_type>)
    {
        throw std::runtime_error("Address::as() conversion failure"); // 抛出异常
    }
    return reinterpret_cast<const sockaddr_type *>(raw); // NOLINT(*-reinterpret-cast)
}

// 显式实例化模板
template const sockaddr_in *Address::as<sockaddr_in>() const;
template const sockaddr_in6 *Address::as<sockaddr_in6>() const;
template const sockaddr_ll *Address::as<sockaddr_ll>() const;
