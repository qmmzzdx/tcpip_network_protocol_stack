#ifndef LOSSY_FD_ADAPTER_H
#define LOSSY_FD_ADAPTER_H

#include <optional>           // 包含 std::optional 的定义，用于表示可能缺失的值
#include <random>             // 包含随机数生成器的定义
#include <utility>            // 包含一些实用工具的定义，如 std::move
#include "file_descriptor.h"  // 包含文件描述符适配器的定义
#include "random.h"           // 包含随机数生成的定义
#include "tcp_config.h"       // 包含 TCP 配置的定义
#include "tcp_segment.h"      // 包含 TCP 段的定义

// 定义一个模板类 LossyFdAdapter，适用于任何类型的适配器
template <typename AdapterT>
class LossyFdAdapter
{
private:
    // 使用默认随机数生成器，初始化时调用全局的随机数生成器
    std::default_random_engine _rand{get_random_engine()};

    // 底层的文件描述符适配器实例
    AdapterT _adapter;

    // 判断是否丢弃读取或写入的数据
    // 参数 uplink 为 true 时使用上行丢包概率，否则使用下行丢包概率
    bool _should_drop(bool uplink)
    {
        // 获取适配器的配置
        const auto &cfg = _adapter.config();
        // 根据 uplink 参数选择丢包率
        const uint16_t loss = uplink ? cfg.loss_rate_up : cfg.loss_rate_dn;
        // 生成随机数并与丢包率进行比较，决定是否丢弃
        return loss != 0 && static_cast<uint16_t>(_rand()) < loss;
    }

public:
    // 返回底层适配器的文件描述符
    FileDescriptor &fd() { return _adapter.fd(); }

    // 构造函数，接受一个适配器实例并初始化底层适配器
    explicit LossyFdAdapter(AdapterT &&adapter) : _adapter(std::move(adapter)) {}

    // 从底层适配器读取数据，可能会丢弃读取的数据报
    // 返回 std::optional<TCPMessage>，如果段被丢弃或底层适配器返回空值则为空
    std::optional<TCPMessage> read()
    {
        // 从底层适配器读取数据
        auto ret = _adapter.read();
        // 检查是否丢弃读取的数据
        if (_should_drop(false))
        {
            return {}; // 如果丢弃，返回空
        }
        return ret; // 返回读取的结果
    }

    // 向底层适配器写入数据，可能会丢弃要写入的数据报
    // 参数 seg 是要写入或丢弃的数据包
    void write(const TCPMessage &seg)
    {
        // 检查是否丢弃写入的数据
        if (_should_drop(true))
        {
            return; // 如果丢弃，直接返回
        }
        // 否则，写入数据
        return _adapter.write(seg);
    }

    // 传递给底层适配器的函数，设置监听状态
    void set_listening(const bool l) { _adapter.set_listening(l); }

    // 返回底层适配器的配置（只读）
    const FdAdapterConfig &config() const { return _adapter.config(); }

    // 返回底层适配器的可变配置
    FdAdapterConfig &config_mut() { return _adapter.config_mut(); }

    // 传递 tick 函数，更新适配器状态
    void tick(const size_t ms_since_last_tick) { _adapter.tick(ms_since_last_tick); }
};

#endif
