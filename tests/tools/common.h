#ifndef COMMON_H
#define COMMON_H

#include <stdexcept>     // 用于 std::runtime_error
#include <memory>        // 用于智能指针
#include <string>        // 用于 std::string
#include <typeinfo>      // 用于 typeid
#include <utility>       // 用于 std::move
#include <vector>        // 用于 std::vector
#include "conversions.h" // 包含类型转换相关的函数
#include "exception.h"   // 包含异常处理相关的类

// 定义 ExpectationViolation 异常类，继承自 std::runtime_error
class ExpectationViolation : public std::runtime_error
{
public:
    // 静态方法，将布尔值转换为字符串
    static constexpr std::string boolstr(bool b) { return b ? "true" : "false"; }

    // 构造函数，接受一个字符串消息
    explicit ExpectationViolation(const std::string &msg) : std::runtime_error(msg) {}

    // 模板构造函数，接受属性名、期望值和实际值
    template <typename T>
    inline ExpectationViolation(const std::string &property_name, const T &expected, const T &actual);
};

// 模板构造函数的实现
template <typename T>
ExpectationViolation::ExpectationViolation(const std::string &property_name, const T &expected, const T &actual)
    : ExpectationViolation{"The object should have had " + property_name + " = " + to_string(expected) + ", but instead it was " + to_string(actual) + "."}
{
}

// 特化构造函数，用于处理布尔值
template <>
inline ExpectationViolation::ExpectationViolation(const std::string &property_name,
                                                  const bool &expected,
                                                  const bool &actual)
    : ExpectationViolation{"The object should have had " + property_name + " = " + boolstr(expected) + ", but instead it was " + boolstr(actual) + "."}
{
}

// 定义一个测试步骤的抽象基类
template <class T>
struct TestStep
{
    // 纯虚函数，返回步骤的字符串描述
    virtual std::string str() const = 0;
    // 纯虚函数，执行步骤
    virtual void execute(T &) const = 0;
    // 纯虚函数，返回步骤的颜色
    virtual uint8_t color() const = 0;
    // 虚析构函数
    virtual ~TestStep() = default;
};

// 定义一个打印类
class Printer
{
    bool is_terminal_; // 是否为终端

public:
    Printer(); // 构造函数

    // 定义颜色常量
    static constexpr int red = 31;
    static constexpr int green = 32;
    static constexpr int blue = 34;
    static constexpr int def = 39;

    // 带颜色的字符串
    std::string with_color(int color_value, std::string_view str) const;

    // 美化字符串，限制最大长度
    static std::string prettify(std::string_view str, size_t max_length = 32);

    // 诊断信息，输出测试名称、执行步骤、失败步骤和异常
    void diagnostic(std::string_view test_name,
                    const std::vector<std::pair<std::string, int>> &steps_executed,
                    const std::string &failing_step,
                    const std::exception &e) const;
};

// 定义测试工具类
template <class T>
class TestHarness
{
    std::string test_name_; // 测试名称
    T obj_;                 // 被测试对象

    std::vector<std::pair<std::string, int>> steps_executed_{};   // 执行的步骤
    Printer pr_{};                                                // 打印对象

protected:
    // 构造函数，初始化测试名称和对象
    explicit TestHarness(std::string test_name, std::string_view desc, T &&object)
        : test_name_(std::move(test_name)), obj_(std::move(object))
    {
        // 记录初始化步骤
        steps_executed_.emplace_back("Initialized " + demangle(typeid(T).name()) + " with " + std::string{desc},
                                     Printer::def);
    }

    // 获取被测试对象的常量引用
    const T &object() const { return obj_; }

public:
    // 执行测试步骤
    void execute(const TestStep<T> &step)
    {
        try
        {
            step.execute(obj_);                                     // 执行步骤
            steps_executed_.emplace_back(step.str(), step.color()); // 记录步骤
        }
        catch (const ExpectationViolation &e) // 捕获 ExpectationViolation 异常
        {
            pr_.diagnostic(test_name_, steps_executed_, step.str(), e);          // 输出诊断信息
            throw std::runtime_error{"The test \"" + test_name_ + "\" failed."}; // 抛出运行时异常
        }
        catch (const std::exception &e) // 捕获其他异常
        {
            pr_.diagnostic(test_name_, steps_executed_, step.str(), e);                                     // 输出诊断信息
            throw std::runtime_error{"The test \"" + test_name_ + "\" made your code throw an exception."}; // 抛出运行时异常
        }
    }
};

// 定义期望的抽象基类
template <class T>
struct Expectation : public TestStep<T>
{
    std::string str() const override { return "Expectation: " + description(); } // 返回期望描述
    virtual std::string description() const = 0;                                 // 纯虚函数，返回描述
    uint8_t color() const override { return Printer::green; }                    // 返回颜色
};

// 定义动作的抽象基类
template <class T>
struct Action : public TestStep<T>
{
    std::string str() const override { return "Action: " + description(); } // 返回动作描述
    virtual std::string description() const = 0;                            // 纯虚函数，返回描述
    uint8_t color() const override { return Printer::blue; }                // 返回颜色
};

// 定义期望数字的结构
template <class T, typename Num>
struct ExpectNumber : public Expectation<T>
{
    Num value_;                                         // 期望的值
    explicit ExpectNumber(Num value) : value_(value) {} // 构造函数

    // 返回描述
    std::string description() const override
    {
        if constexpr (std::is_same<Num, bool>::value) // 如果 Num 是 bool 类型
        {
            return name() + " = " + ExpectationViolation::boolstr(value_); // 返回布尔值描述
        }
        else
        {
            return name() + " = " + to_string(value_); // 返回其他类型的描述
        }
    }

    virtual std::string name() const = 0; // 纯虚函数，返回名称
    virtual Num value(T &) const = 0;     // 纯虚函数，返回值
    void execute(T &obj) const override   // 执行期望检查
    {
        const Num result{value(obj)}; // 获取实际值
        if (result != value_)         // 如果实际值与期望值不匹配
        {
            throw ExpectationViolation{name(), value_, result}; // 抛出 ExpectationViolation 异常
        }
    }
};

// 定义常量期望数字的结构
template <class T, typename Num>
struct ConstExpectNumber : public ExpectNumber<T, Num>
{
    using ExpectNumber<T, Num>::ExpectNumber; // 继承构造函数
    using ExpectNumber<T, Num>::execute;      // 继承 execute 方法
    using ExpectNumber<T, Num>::name;         // 继承 name 方法

    void execute(const T &obj) const // 执行期望检查（常量对象）
    {
        const Num result{value(obj)};               // 获取实际值
        if (result != ExpectNumber<T, Num>::value_) // 如果实际值与期望值不匹配
        {
            throw ExpectationViolation{name(), ExpectNumber<T, Num>::value_, result}; // 抛出 ExpectationViolation 异常
        }
    }

    Num value(T &obj) const override { return value(std::as_const(obj)); } // 获取常量对象的值
    virtual Num value(const T &) const = 0;                                // 纯虚函数，返回值
};

// 定义期望布尔值的结构
template <class T>
struct ExpectBool : public ExpectNumber<T, bool>
{
    using ExpectNumber<T, bool>::ExpectNumber; // 继承构造函数
};

// 定义常量期望布尔值的结构
template <class T>
struct ConstExpectBool : public ConstExpectNumber<T, bool>
{
    using ConstExpectNumber<T, bool>::ConstExpectNumber; // 继承构造函数
};

#endif
