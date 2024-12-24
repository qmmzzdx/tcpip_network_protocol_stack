#ifndef TUNTAP_ADAPTER_H
#define TUNTAP_ADAPTER_H

#include <utility>        // 包含一些实用工具的库
#include <optional>       // 包含可选类型的库
#include <unordered_map>  // 包含无序映射的库
#include "tun.h"          // 包含 TUN 设备的定义
#include "tcp_over_ip.h"  // 包含 TCP 通过 IP 的适配器定义
#include "tcp_segment.h"  // 包含 TCP 段的定义

// 定义一个概念，约束 TCP 数据报适配器的类型
template <class T>
concept TCPDatagramAdapter = requires(T a, TCPMessage seg) {
    {
        a.write(seg)         // 要求适配器能够写入 TCP 消息
    } -> std::same_as<void>; // 返回类型必须是 void

    {
        a.read()                                  // 要求适配器能够读取 TCP 消息
    } -> std::same_as<std::optional<TCPMessage>>; // 返回类型必须是 std::optional<TCPMessage>
};

// 定义一个类，表示通过 TUN 设备的 TCP 适配器
class TCPOverIPv4OverTunFdAdapter : public TCPOverIPv4Adapter
{
private:
    TunFD _tun; // TUN 文件描述符

public:
    // 构造函数，接受一个右值引用的 TunFD 对象
    explicit TCPOverIPv4OverTunFdAdapter(TunFD &&tun) : _tun(std::move(tun)) {}

    // 读取 TCP 消息，返回一个可选的 TCP 消息
    std::optional<TCPMessage> read();

    // 写入 TCP 消息
    void write(const TCPMessage &seg)
    {
        _tun.write(serialize(wrap_tcp_in_ip(seg))); // 将 TCP 消息封装在 IP 中并写入 TUN 设备
    }

    // 将适配器转换为 TunFD 的引用
    explicit operator TunFD &() { return _tun; }

    // 将适配器转换为常量 TunFD 的引用
    explicit operator const TunFD &() const { return _tun; }

    // 获取底层文件描述符
    FileDescriptor &fd() { return _tun; }
};

// 静态断言，确保 TCPOverIPv4OverTunFdAdapter 满足 TCPDatagramAdapter 概念
static_assert(TCPDatagramAdapter<TCPOverIPv4OverTunFdAdapter>);
// 静态断言，确保 LossyFdAdapter<TCPOverIPv4OverTunFdAdapter> 满足 TCPDatagramAdapter 概念
static_assert(TCPDatagramAdapter<LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>>);

#endif
