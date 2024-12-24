#include <iostream>
#include <iomanip>
#include <unistd.h>
#include "common.h"
using namespace std;

// 打印诊断信息的函数
void Printer::diagnostic(std::string_view test_name,
                         const vector<pair<string, int>> &steps_executed,
                         const string &failing_step,
                         const exception &e) const
{
    const string quote = Printer::with_color(Printer::def, "\""); // 用于格式化引号
    cerr << "\nThe test " << quote << Printer::with_color(Printer::def, test_name) << quote
         << " failed after these steps:\n\n"; // 输出测试名称和失败信息

    unsigned int step_num = 0;                    // 步骤编号
    for (const auto &[str, col] : steps_executed) // 遍历已执行的步骤
    {
        cerr << "  " << step_num++ << "."
             << "\t" << with_color(col, str) << "\n"; // 输出步骤编号和步骤内容
    }
    cerr << with_color(red, "  ***** Unsuccessful " + failing_step + " *****\n\n"); // 输出失败的步骤

    // 输出异常信息
    cerr << with_color(red, demangle(typeid(e).name())) << ": " << with_color(def, e.what()) << "\n\n";
}

// 格式化字符串的函数，限制最大长度
string Printer::prettify(string_view str, size_t max_length)
{
    ostringstream ss;                                         // 用于构建输出字符串
    const string_view str_prefix = str.substr(0, max_length); // 截取前 max_length 个字符
    for (const uint8_t ch : str_prefix)                       // 遍历字符
    {
        if (isprint(ch)) // 如果字符可打印
        {
            ss << ch; // 直接添加到输出
        }
        else // 如果字符不可打印
        {
            // 以十六进制格式输出不可打印字符
            ss << "\\x" << fixed << setw(2) << setfill('0') << hex << static_cast<size_t>(ch);
        }
    }
    if (str.size() > str_prefix.size()) // 如果原字符串超过最大长度
    {
        ss << "..."; // 添加省略号
    }
    return ss.str(); // 返回格式化后的字符串
}

// Printer 类的构造函数
Printer::Printer() : is_terminal_(isatty(STDERR_FILENO) or getenv("MAKE_TERMOUT"))
{
    // 检查标准错误输出是否为终端，或环境变量 MAKE_TERMOUT 是否存在
}

// 用于带颜色输出的函数
string Printer::with_color(int color_value, string_view str) const
{
    string ret;       // 存储最终输出的字符串
    if (is_terminal_) // 如果是在终端中
    {
        ret += "\033[1;" + to_string(color_value) + "m"; // 添加颜色前缀
    }
    ret += str;       // 添加要输出的字符串
    if (is_terminal_) // 如果是在终端中
    {
        ret += "\033[m"; // 添加颜色重置后缀
    }
    return ret; // 返回带颜色的字符串
}
