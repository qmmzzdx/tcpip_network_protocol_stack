#ifndef TEST_SHOULD_BE_H
#define TEST_SHOULD_BE_H

#include <cstdint>       // 用于整数类型
#include <optional>      // 用于 std::optional
#include <sstream>       // 用于 std::ostringstream
#include <stdexcept>     // 用于 std::runtime_error
#include <string>        // 用于 std::string
#include "conversions.h" // 包含类型转换相关的函数

// 定义宏 test_should_be，用于简化测试实际值与期望值的比较
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): 该注释用于禁用特定的代码检查
#define test_should_be(act, exp) test_should_be_helper(act, exp, #act, #exp, __LINE__)

// 辅助函数，比较实际值与期望值
template <typename T>
static void test_should_be_helper(const T &actual, const T &expected, const char *actual_s, const char *expected_s, const int lineno)
{
    // 如果实际值与期望值不相等
    if (actual != expected)
    {
        std::ostringstream ss; // 创建字符串流
        // 构建错误消息
        ss << "`" << actual_s << "` should have been `" << expected_s << "`, but the former is\n\t"
           << to_string(actual) << "\nand the latter is\n\t" << to_string(expected) << " (difference of "
           << static_cast<int64_t>(expected - actual) << ")\n"
           << " (at line " << lineno << ")\n";
        // 抛出运行时异常，包含错误消息
        throw std::runtime_error(ss.str());
    }
}

#endif
