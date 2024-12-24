#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <cxxabi.h>     // 用于 C++ 名称解码
#include <cerrno>       // 提供 errno 变量
#include <memory>       // 提供智能指针
#include <string>       // 提供字符串类
#include <string_view>  // 提供字符串视图类
#include <system_error> // 提供系统错误处理

// 自定义异常类 tagged_error，继承自 std::system_error
class tagged_error : public std::system_error
{
private:
    std::string attempt_and_error_; // 存储尝试的操作和错误信息
    int error_code_;                // 存储错误代码

public:
    // 构造函数，接受错误类别、尝试的操作和错误代码
    tagged_error(const std::error_category &category, const std::string_view s_attempt, const int error_code)
        : system_error(error_code, category),                                            // 调用基类构造函数
          attempt_and_error_(std::string(s_attempt) + ": " + std::system_error::what()), // 组合错误信息
          error_code_(error_code)                                                        // 初始化错误代码
    {
    }

    // 重写 what() 方法，返回详细的错误信息
    const char *what() const noexcept override { return attempt_and_error_.c_str(); }

    // 返回错误代码
    int error_code() const { return error_code_; }
};

// 自定义异常类 unix_error，专门用于处理 UNIX 系统错误
class unix_error : public tagged_error
{
public:
    // 构造函数，接受尝试的操作和可选的 errno 值
    explicit unix_error(const std::string_view s_attempt, const int s_errno = errno) : tagged_error(std::system_category(), s_attempt, s_errno) {} // 调用基类构造函数
};

// 检查系统调用的返回值，如果失败则抛出 unix_error 异常
inline int CheckSystemCall(const std::string_view s_attempt, const int return_value)
{
    if (return_value >= 0) // 如果返回值有效
    {
        return return_value; // 返回有效值
    }
    throw unix_error{s_attempt}; // 否则抛出异常
}

// 检查指针是否为 nullptr，如果是则抛出异常
template <typename T>
inline T *notnull(const std::string_view context, T *const x)
{
    return x ? x : throw std::runtime_error(std::string(context) + ": returned null pointer");
}

// 检查 std::unique_ptr 是否有效，如果无效则抛出异常
template <typename T>
inline std::unique_ptr<T> notnull(const std::string_view context, std::unique_ptr<T> x)
{
    return x ? x : throw std::runtime_error(std::string(context) + ": returned null pointer");
}

// 检查 std::shared_ptr 是否有效，如果无效则抛出异常
template <typename T>
inline std::shared_ptr<T> notnull(const std::string_view context, std::shared_ptr<T> x)
{
    return x ? x : throw std::runtime_error(std::string(context) + ": returned null pointer");
}

// 解码 C++ mangled 名称的函数
inline std::string demangle(const char *name)
{
    int status = 0;                                                                                               // 状态变量，用于存储解码状态
    const std::unique_ptr<char, decltype(&free)> res{abi::__cxa_demangle(name, nullptr, nullptr, &status), free}; // 调用解码函数并管理内存

    if (status != 0) // 检查解码是否成功
    {
        throw std::runtime_error("cxa_demangle"); // 如果失败，抛出异常
    }
    return res.get(); // 返回解码后的字符串
}

#endif // EXCEPTION_H
