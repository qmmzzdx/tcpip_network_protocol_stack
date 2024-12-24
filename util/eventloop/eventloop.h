#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <ostream>
#include <list>
#include <memory>
#include <poll.h>
#include <string_view>
#include <functional>
#include "file_descriptor.h"

// EventLoop 类用于监视文件描述符的事件并执行相应的回调
class EventLoop
{
public:
    // 表示对文件描述符的读写兴趣。
    enum class Direction : int16_t
    {
        In = POLLIN,  // 当文件描述符可读时触发回调
        Out = POLLOUT // 当文件描述符可写时触发回调
    };

private:
    using CallbackT = std::function<void(void)>; // 定义回调函数类型
    using InterestT = std::function<bool(void)>; // 定义兴趣函数类型

    // 规则类别结构体
    struct RuleCategory
    {
        std::string name; // 规则类别的名称
    };

    // 基本规则结构体
    struct BasicRule
    {
        size_t category_id;      // 规则所属的类别 ID
        InterestT interest;      // 规则的兴趣函数
        CallbackT callback;      // 规则的回调函数
        bool cancel_requested{}; // 是否请求取消

        // 构造函数
        BasicRule(size_t s_category_id, InterestT s_interest, CallbackT s_callback);
    };

    // 文件描述符规则结构体
    struct FDRule : public BasicRule
    {
        FileDescriptor fd;   // 监视活动的文件描述符
        Direction direction; // 方向：Direction::In 表示读取，Direction::Out 表示写入
        CallbackT cancel;    // 当规则被取消时调用的回调（例如，EOF 或挂起）
        CallbackT error;     // 当文件描述符在取消之前发生错误时调用的回调

        // 构造函数
        FDRule(BasicRule &&base, FileDescriptor &&s_fd, Direction s_direction, CallbackT s_cancel, CallbackT s_error);

        //! 返回根据方向读取或写入文件描述符的次数。
        unsigned int service_count() const;
    };

    std::vector<RuleCategory> _rule_categories{};          // 存储规则类别的向量
    std::list<std::shared_ptr<FDRule>> _fd_rules{};        // 存储文件描述符规则的列表
    std::list<std::shared_ptr<BasicRule>> _non_fd_rules{}; // 存储非文件描述符规则的列表

public:
    // 构造函数，预留空间以存储规则类别
    EventLoop() { _rule_categories.reserve(64); }

    // wait_next_event 函数的返回结果
    enum class Result
    {
        Success, // 至少有一个规则被触发
        Timeout, // 在超时之前没有规则被触发
        Exit     // 所有规则已被取消或不感兴趣，后续调用将无效
    };

    // 添加规则类别
    size_t add_category(const std::string &name);

    // 规则句柄类
    class RuleHandle
    {
    private:
        std::weak_ptr<BasicRule> rule_weak_ptr_; // 弱指针，避免循环引用

    public:
        // 构造函数
        template <class RuleType>
        explicit RuleHandle(const std::shared_ptr<RuleType> x) : rule_weak_ptr_(x) {}
        void cancel(); // 取消规则
    };

    // 添加文件描述符规则
    RuleHandle add_rule(
        size_t category_id,
        FileDescriptor &fd,
        Direction direction,
        const CallbackT &callback,
        const InterestT &interest = [] { return true; },  // 默认兴趣函数
        const CallbackT &cancel = [] {},                  // 默认取消回调
        const CallbackT &error = [] {}                    // 默认错误回调
    );

    // 添加非文件描述符规则
    RuleHandle add_rule(
        size_t category_id,
        const CallbackT &callback,
        const InterestT &interest = [] { return true; }   // 默认兴趣函数
    );

    // 调用 poll 函数并执行每个就绪文件描述符的回调
    Result wait_next_event(int timeout_ms);

    // 允许同时添加类别和规则
    template <typename... Targs>
    auto add_rule(const std::string &name, Targs &&...Fargs)
    {
        return add_rule(add_category(name), std::forward<Targs>(Fargs)...);
    }
};

// 为 Direction 创建别名
using Direction = EventLoop::Direction;

#endif
