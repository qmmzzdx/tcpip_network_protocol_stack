#ifndef FD_ADAPTER_H
#define FD_ADAPTER_H

#include <optional>            // 包含可选类型的库
#include <utility>             // 包含一些实用工具的库
#include "file_descriptor.h"   // 包含文件描述符的定义
#include "lossy_fd_adapter.h"  // 包含有损文件描述符适配器的定义
#include "socket.h"            // 包含套接字的定义
#include "tcp_config.h"        // 包含 TCP 配置的定义
#include "tcp_segment.h"       // 包含 TCP 段的定义

// 文件描述符适配器基类
class FdAdapterBase
{
private:
    FdAdapterConfig _cfg{}; // 文件描述符适配器的配置，使用默认构造函数初始化
    bool _listen = false;   // 监听状态，默认为 false

protected:
    // 返回可修改的配置引用
    FdAdapterConfig &config_mutable() { return _cfg; }

public:
    // 设置监听状态
    void set_listening(const bool l) { _listen = l; }

    // 获取当前监听状态
    bool listening() const { return _listen; }

    // 获取当前配置的常量引用
    const FdAdapterConfig &config() const { return _cfg; }

    // 获取当前配置的可修改引用
    FdAdapterConfig &config_mut() { return _cfg; }

    // 每个 tick 的操作，参数 unused 表示未使用的大小
    void tick(const size_t unused [[maybe_unused]]) {}
};

#endif
