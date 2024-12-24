#ifndef CONVERSIONS_H
#define CONVERSIONS_H

#include <optional>            // 用于 std::optional
#include <string>              // 用于 std::string
#include <utility>             // 用于 std::forward
#include "wrapping_integers.h" // 包含 Wrap32 类的定义

namespace minnow_conversions
{
    using std::to_string; // 使用 std::to_string 函数

    // 定义 DebugWrap32 类，继承自 Wrap32
    class DebugWrap32 : public Wrap32
    {
    public:
        // 获取原始值的调试方法
        uint32_t debug_get_raw_value() { return raw_value_; }
    };

    // 将 Wrap32 类型转换为字符串
    inline std::string to_string(Wrap32 i)
    {
        return "Wrap32<" + std::to_string(DebugWrap32{i}.debug_get_raw_value()) + ">"; // 返回格式化字符串
    }

    // 将 std::optional<T> 类型转换为字符串
    template <typename T>
    std::string to_string(const std::optional<T> &v)
    {
        if (v.has_value()) // 检查是否有值
        {
            return "Some(" + to_string(v.value()) + ")"; // 有值时返回 "Some(value)"
        }

        return "None"; // 没有值时返回 "None"
    }
}

// 定义一个概念，要求类型 T 必须可转换为字符串
template <typename T>
concept MinnowStringable = requires(T t) { minnow_conversions::to_string(t); };

// 定义一个模板函数，将类型 T 转换为字符串
template <MinnowStringable T>
std::string to_string(T &&t)
{
    return minnow_conversions::to_string(std::forward<T>(t)); // 使用完美转发
}

// 重载输出流操作符，将 Wrap32 类型输出到流
inline std::ostream &operator<<(std::ostream &os, Wrap32 a)
{
    return os << to_string(a); // 调用 to_string 函数
}

// 重载不等于操作符
inline bool operator!=(Wrap32 a, Wrap32 b)
{
    return not(a == b); // 使用 not 关键字返回不等于的结果
}

// 重载减法操作符，返回 Wrap32 对象的原始值之差
inline int64_t operator-(Wrap32 a, Wrap32 b)
{
    return static_cast<int64_t>(minnow_conversions::DebugWrap32{a}.debug_get_raw_value()) -
           static_cast<int64_t>(minnow_conversions::DebugWrap32{b}.debug_get_raw_value());
}

#endif
