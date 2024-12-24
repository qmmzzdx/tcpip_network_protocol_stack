#ifndef ADDRESS_H
#define ADDRESS_H

#include <cstddef>      // 提供 std::size_t
#include <cstdint>      // 提供固定宽度整数类型
#include <netdb.h>      // 提供网络数据库操作
#include <string>       // 提供 std::string 类
#include <utility>      // 提供 std::pair
#include <stdexcept>    // 提供标准异常类
#include <netinet/in.h> // 提供网络地址结构
#include <sys/socket.h> // 提供 socket 操作

// Address 类用于表示网络地址
class Address
{
public:
    // Raw 类用于封装 sockaddr_storage 结构
    class Raw
    {
    public:
        sockaddr_storage storage{}; // 存储网络地址信息

        // NOLINTBEGIN (*-explicit-*)
        operator sockaddr *();             // 类型转换运算符，将 Raw 转换为 sockaddr*
        operator const sockaddr *() const; // 类型转换运算符，将 Raw 转换为 const sockaddr*
        // NOLINTEND (*-explicit-*)
    };

private:
    socklen_t _size; // 地址的大小
    Raw _address{};  // 存储地址的 Raw 对象

    // 私有构造函数，使用节点、服务和地址信息结构初始化 Address
    Address(const std::string &node, const std::string &service, const addrinfo &hints);

public:
    // 构造函数，使用主机名和服务名初始化 Address
    Address(const std::string &hostname, const std::string &service);

    // 构造函数，使用 IP 地址和端口号初始化 Address
    explicit Address(const std::string &ip, std::uint16_t port = 0);

    // 构造函数，使用 sockaddr 指针和大小初始化 Address
    Address(const sockaddr *addr, std::size_t size);

    // 重载相等运算符
    bool operator==(const Address &other) const;
    // 重载不等运算符
    bool operator!=(const Address &other) const { return !operator==(other); }

    // 返回 IP 地址和端口号的配对
    std::pair<std::string, uint16_t> ip_port() const;

    // 返回 IP 地址
    std::string ip() const { return ip_port().first; }

    // 返回端口号
    uint16_t port() const { return ip_port().second; }

    // 返回 IPv4 地址的数值表示
    uint32_t ipv4_numeric() const;

    // 从 IPv4 数值地址创建 Address 对象
    static Address from_ipv4_numeric(uint32_t ip_address);

    // 将地址转换为字符串表示
    std::string to_string() const;

    // 返回地址的大小
    socklen_t size() const { return _size; }

    // 返回原始 sockaddr 指针
    const sockaddr *raw() const { return static_cast<const sockaddr *>(_address); }

    // 将 Address 转换为指定类型的 sockaddr*
    template <typename sockaddr_type>
    const sockaddr_type *as() const;
};

#endif // ADDRESS_H
