#include <iostream>
#include <cstring>
#include <iomanip>
#include "socket.h"
#include "eventloop.h"
#include "exception.h"

// 返回文件描述符的服务计数，取决于方向（可读或可写）
unsigned int EventLoop::FDRule::service_count() const
{
    return direction == Direction::In ? fd.read_count() : fd.write_count();
}

// 添加规则类别
size_t EventLoop::add_category(const std::string &name)
{
    // 检查是否达到最大类别数
    if (_rule_categories.size() >= _rule_categories.capacity())
    {
        throw std::runtime_error("maximum categories reached");
    }
    // 添加类别并返回其 ID
    _rule_categories.push_back({name});
    return _rule_categories.size() - 1;
}

// BasicRule 构造函数
EventLoop::BasicRule::BasicRule(size_t s_category_id, InterestT s_interest, CallbackT s_callback)
    : category_id(s_category_id), interest(std::move(s_interest)), callback(std::move(s_callback))
{
}

// FDRule 构造函数
EventLoop::FDRule::FDRule(BasicRule &&base, FileDescriptor &&s_fd, Direction s_direction, CallbackT s_cancel, CallbackT s_error)
    : BasicRule(base), fd(std::move(s_fd)), direction(s_direction), cancel(std::move(s_cancel)), error(std::move(s_error))
{
}

// 添加文件描述符规则
EventLoop::RuleHandle EventLoop::add_rule(size_t category_id,
                                          FileDescriptor &fd,
                                          Direction direction,
                                          const CallbackT &callback,
                                          const InterestT &interest,
                                          const CallbackT &cancel, // NOLINT(*-easily-swappable-*)
                                          const CallbackT &error)
{
    // 检查类别 ID 是否有效
    if (category_id >= _rule_categories.size())
    {
        throw std::out_of_range("bad category_id");
    }
    // 创建 FDRule 并添加到规则列表
    _fd_rules.emplace_back(std::make_shared<FDRule>(BasicRule{category_id, interest, callback}, fd.duplicate(), direction, cancel, error));
    return RuleHandle{_fd_rules.back()};
}

// 添加非文件描述符规则
EventLoop::RuleHandle EventLoop::add_rule(const size_t category_id,
                                          const CallbackT &callback,
                                          const InterestT &interest)
{
    // 检查类别 ID 是否有效
    if (category_id >= _rule_categories.size())
    {
        throw std::out_of_range("bad category_id");
    }
    // 创建 BasicRule 并添加到非文件描述符规则列表
    _non_fd_rules.emplace_back(std::make_shared<BasicRule>(category_id, interest, callback));
    return RuleHandle{_non_fd_rules.back()};
}

// 取消规则
void EventLoop::RuleHandle::cancel()
{
    const std::shared_ptr<BasicRule> rule_shared_ptr = rule_weak_ptr_.lock();
    if (rule_shared_ptr)
    {
        rule_shared_ptr->cancel_requested = true; // 设置取消请求标志
    }
}

// NOLINTBEGIN(*-cognitive-complexity)
// NOLINTBEGIN(*-signed-bitwise)
EventLoop::Result EventLoop::wait_next_event(const int timeout_ms)
{
    // 首先处理与非文件描述符相关的规则
    {
        for (auto it = _non_fd_rules.begin(); it != _non_fd_rules.end();)
        {
            auto &this_rule = **it;  // 获取当前规则
            bool rule_fired = false; // 标记规则是否被触发

            // 检查是否请求取消
            if (this_rule.cancel_requested)
            {
                it = _non_fd_rules.erase(it); // 删除已取消的规则
                continue;
            }

            uint8_t iterations = 0;      // 迭代计数
            while (this_rule.interest()) // 如果规则仍然感兴趣
            {
                if (iterations++ >= 128) // 防止忙等待
                {
                    throw std::runtime_error("EventLoop: busy wait detected: rule \"" + _rule_categories.at(this_rule.category_id).name + "\" is still interested after " + std::to_string(iterations) + " iterations");
                }
                rule_fired = true;    // 标记规则被触发
                this_rule.callback(); // 执行回调
            }
            if (rule_fired)
            {
                return Result::Success; /* 每次迭代只处理一个规则 */
            }
            ++it; // 移动到下一个规则
        }
    }

    // 现在处理与文件描述符相关的规则，轮询所有“感兴趣”的文件描述符
    std::vector<pollfd> pollfds{};
    pollfds.reserve(_fd_rules.size()); // 预留空间
    bool something_to_poll = false;    // 标记是否有需要轮询的文件描述符

    // 为每个规则设置 pollfd
    for (auto it = _fd_rules.begin(); it != _fd_rules.end();)
    { // NOTE: 在循环体内可能会删除或递增 it
        auto &this_rule = **it;

        // 检查是否请求取消
        if (this_rule.cancel_requested)
        {
            it = _fd_rules.erase(it); // 删除已取消的规则
            continue;
        }

        // 检查文件描述符是否到达 EOF
        if (this_rule.direction == Direction::In && this_rule.fd.eof())
        {
            this_rule.cancel();       // 取消规则
            it = _fd_rules.erase(it); // 删除规则
            continue;
        }

        // 检查文件描述符是否关闭
        if (this_rule.fd.closed())
        {
            this_rule.cancel();       // 取消规则
            it = _fd_rules.erase(it); // 删除规则
            continue;
        }

        // 如果规则感兴趣，则添加到 pollfds
        if (this_rule.interest())
        {
            pollfds.push_back({this_rule.fd.fd_num(), static_cast<int16_t>(this_rule.direction), 0});
            something_to_poll = true; // 标记有需要轮询的文件描述符
        }
        else
        {
            pollfds.push_back({this_rule.fd.fd_num(), 0, 0}); // 占位符 --- 我们仍然想要错误
        }
        ++it; // 移动到下一个规则
    }

    // 如果没有需要轮询的文件描述符，则退出
    if (!something_to_poll)
    {
        return Result::Exit;
    }

    // 调用 poll -- 等待直到一个文件描述符满足一个规则（可写/可读）
    if (0 == CheckSystemCall("poll", ::poll(pollfds.data(), pollfds.size(), timeout_ms)))
    {
        return Result::Timeout; // 超时
    }

    // 遍历 poll 结果
    for (auto [it, idx] = std::make_pair(_fd_rules.begin(), static_cast<size_t>(0)); it != _fd_rules.end(); ++idx)
    {
        const auto &this_pollfd = pollfds.at(idx); // 获取当前 pollfd
        auto &this_rule = **it;                    // 获取当前规则

        // 检查是否发生错误
        const auto poll_error = static_cast<bool>(this_pollfd.revents & (POLLERR | POLLNVAL));
        if (poll_error)
        {
            /* 检查文件描述符是否是一个套接字 */
            int socket_error = 0;
            socklen_t optlen = sizeof(socket_error);
            const int ret = getsockopt(this_rule.fd.fd_num(), SOL_SOCKET, SO_ERROR, &socket_error, &optlen);
            if (ret == -1 && errno == ENOTSOCK)
            {
                std::cerr << "error on polled file descriptor for rule \"" << _rule_categories.at(this_rule.category_id).name << "\"\n";
            }
            else if (ret == -1)
            {
                throw unix_error("getsockopt");
            }
            else if (optlen != sizeof(socket_error))
            {
                throw std::runtime_error("unexpected length from getsockopt: " + std::to_string(optlen));
            }
            else if (socket_error)
            {
                std::cerr << "error on polled socket for rule \"" << _rule_categories.at(this_rule.category_id).name;
                std::cerr << "\": " << strerror(socket_error) << "\n";
            }
            this_rule.error();        // 执行错误回调
            this_rule.cancel();       // 取消规则
            it = _fd_rules.erase(it); // 删除规则
            continue;
        }

        // 检查文件描述符是否准备好
        const auto poll_ready = static_cast<bool>(this_pollfd.revents & this_pollfd.events);
        const auto poll_hup = static_cast<bool>(this_pollfd.revents & POLLHUP);
        if (poll_hup && ((this_pollfd.events && !poll_ready) || (this_rule.direction == Direction::Out)))
        {
            // 如果我们请求状态，并且唯一的条件是挂起，则此文件描述符无效：
            //   - 如果是 POLLIN 并且没有可读内容，则将永远不会有可读内容
            //   - 如果是 POLLOUT，则将不再可写
            this_rule.cancel();       // 取消规则
            it = _fd_rules.erase(it); // 删除规则
            continue;
        }

        if (poll_ready)
        {
            // 仅当 revents 包含我们请求的事件时才调用回调
            const auto count_before = this_rule.service_count(); // 获取服务计数
            this_rule.callback();                                // 执行回调

            // 检查是否发生忙等待
            if (count_before == this_rule.service_count() && !this_rule.fd.closed() && this_rule.interest())
            {
                throw std::runtime_error("EventLoop: busy wait detected: rule \"" + _rule_categories.at(this_rule.category_id).name + "\" did not read/write fd and is still interested");
            }
            return Result::Success; /* 每次迭代只处理一个规则 */
        }
        ++it; // 如果到这里，意味着我们没有调用 _fd_rules.erase()
    }
    return Result::Success; // 返回成功
}
// NOLINTEND(*-signed-bitwise)
// NOLINTEND(*-cognitive-complexity)
