#include <cstdint>                     // 包含 C++ 标准库的整数类型定义
#include <cstdlib>                     // 包含 C 标准库的头文件，提供通用工具函数
#include <cstring>                     // 包含字符串处理的头文件
#include <iostream>                    // 包含输入输出流的头文件
#include <random>                      // 包含随机数生成的头文件
#include <span>                        // C++20 的 span 头文件，用于处理数组视图
#include <string>                      // 包含字符串类的头文件
#include <tuple>                       // 包含元组的头文件
#include "tun.h"                       // 引入 TUN 设备的头文件
#include "tcp_config.h"                // 引入 TCP 配置的头文件
#include "tcp_minnow_socket.h"         // 引入 Minnow socket 的头文件
#include "bidirectional_stream_copy.h" // 引入自定义的头文件，包含双向流复制的函数声明

// 定义常量
constexpr const char *TUN_DFLT = "tun144";                  // 默认 TUN 设备名称
constexpr const char *LOCAL_ADDRESS_DFLT = "169.254.144.9"; // 默认本地地址

namespace
{
    // 显示程序的使用说明
    void show_usage(const char *argv0, const char *msg)
    {
        std::cout << "Usage: " << argv0 << " [options] <host> <port>\n\n";
        std::cout << "   Option                                                          Default\n";
        std::cout << "   --                                                              --\n\n";
        std::cout << "   -l              Server (listen) mode.                           (client mode)\n";
        std::cout << "                   In server mode, <host>:<port> is the address to bind.\n\n";
        std::cout << "   -a <addr>       Set source address (client mode only)           " << LOCAL_ADDRESS_DFLT << "\n";
        std::cout << "   -s <port>       Set source port (client mode only)              (random)\n\n";
        std::cout << "   -w <winsz>      Use a window of <winsz> bytes                   " << TCPConfig::MAX_PAYLOAD_SIZE << "\n\n";
        std::cout << "   -t <tmout>      Set rt_timeout to tmout                         " << TCPConfig::TIMEOUT_DFLT << "\n\n";
        std::cout << "   -d <tundev>     Connect to tun <tundev>                         " << TUN_DFLT << "\n\n";
        std::cout << "   -Lu <loss>      Set uplink loss to <rate> (float in 0..1)       (no loss)\n";
        std::cout << "   -Ld <loss>      Set downlink loss to <rate> (float in 0..1)     (no loss)\n\n";
        std::cout << "   -h              Show this message.\n\n";

        if (msg != nullptr)
        {
            std::cout << msg; // 如果有额外消息，输出该消息
        }
        std::cout << std::endl; // 输出换行
    }

    // 检查参数数量
    void check_argc(const std::span<char *> &args, size_t curr, const char *err)
    {
        if (curr + 3 >= args.size())
        {                                  // 检查是否有足够的参数
            show_usage(args.front(), err); // 显示用法和错误信息
            exit(1);                       // 退出程序
        }
    }

    // 获取配置
    std::tuple<TCPConfig, FdAdapterConfig, bool, const char *> get_config(const std::span<char *> &args)
    {
        TCPConfig c_fsm{};                          // 创建 TCP 配置对象
        c_fsm.isn = Wrap32{std::random_device()()}; // 初始化 ISN（初始序列号）

        FdAdapterConfig c_filt{};     // 创建文件描述符适配器配置对象
        const char *tundev = nullptr; // TUN 设备名称初始化为 nullptr

        size_t curr = 1;                 // 当前参数索引
        bool listen = false;             // 标志变量，指示是否为服务器模式
        const size_t argc = args.size(); // 参数数量

        std::string source_address = LOCAL_ADDRESS_DFLT;                            // 源地址，默认为 LOCAL_ADDRESS_DFLT
        std::string source_port = std::to_string(uint16_t(std::random_device()())); // 源端口，随机生成

        // 解析命令行参数
        while (argc - curr > 2)
        {
            if (std::strncmp("-l", args[curr], 3) == 0)
            {                  // 检查是否为服务器模式
                listen = true; // 设置为服务器模式
                curr += 1;     // 移动到下一个参数
            }
            else if (std::strncmp("-a", args[curr], 3) == 0)
            {                                                               // 检查源地址参数
                check_argc(args, curr, "ERROR: -a requires one argument."); // 检查参数数量
                source_address = args[curr + 1];                            // 设置源地址
                curr += 2;                                                  // 移动到下一个参数
            }
            else if (std::strncmp("-s", args[curr], 3) == 0)
            {                                                               // 检查源端口参数
                check_argc(args, curr, "ERROR: -s requires one argument."); // 检查参数数量
                source_port = args[curr + 1];                               // 设置源端口
                curr += 2;                                                  // 移动到下一个参数
            }
            else if (std::strncmp("-w", args[curr], 3) == 0)
            {                                                                  // 检查窗口大小参数
                check_argc(args, curr, "ERROR: -w requires one argument.");    // 检查参数数量
                c_fsm.recv_capacity = std::strtol(args[curr + 1], nullptr, 0); // 设置接收容量
                curr += 2;                                                     // 移动到下一个参数
            }
            else if (std::strncmp("-t", args[curr], 3) == 0)
            {                                                               // 检查超时参数
                check_argc(args, curr, "ERROR: -t requires one argument."); // 检查参数数量
                c_fsm.rt_timeout = std::strtol(args[curr + 1], nullptr, 0); // 设置超时
                curr += 2;                                                  // 移动到下一个参数
            }
            else if (std::strncmp("-d", args[curr], 3) == 0)
            {                                                               // 检查 TUN 设备参数
                check_argc(args, curr, "ERROR: -d requires one argument."); // 检查参数数量
                tundev = args[curr + 1];                                    // 设置 TUN 设备名称
                curr += 2;                                                  // 移动到下一个参数
            }
            else if (std::strncmp("-Lu", args[curr], 3) == 0)
            {                                                                                                                           // 检查上行丢包率参数
                check_argc(args, curr, "ERROR: -Lu requires one argument.");                                                            // 检查参数数量
                const float lossrate = std::strtof(args[curr + 1], nullptr);                                                            // 获取丢包率
                using LossRateUpT = decltype(c_filt.loss_rate_up);                                                                      // 获取上行丢包率类型
                c_filt.loss_rate_up = static_cast<LossRateUpT>(static_cast<float>(std::numeric_limits<LossRateUpT>::max()) * lossrate); // 设置上行丢包率
                curr += 2;                                                                                                              // 移动到下一个参数
            }
            else if (std::strncmp("-Ld", args[curr], 3) == 0)
            {                                                                                                                           // 检查下行丢包率参数
                check_argc(args, curr, "ERROR: -Ld requires one argument.");                                                            // 检查参数数量
                const float lossrate = std::strtof(args[curr + 1], nullptr);                                                            // 获取丢包率
                using LossRateDnT = decltype(c_filt.loss_rate_dn);                                                                      // 获取下行丢包率类型
                c_filt.loss_rate_dn = static_cast<LossRateDnT>(static_cast<float>(std::numeric_limits<LossRateDnT>::max()) * lossrate); // 设置下行丢包率
                curr += 2;                                                                                                              // 移动到下一个参数
            }
            else if (std::strncmp("-h", args[curr], 3) == 0)
            {                                 // 检查帮助参数
                show_usage(args[0], nullptr); // 显示用法
                exit(0);                      // 退出程序
            }
            else
            {                                                                                                      // 处理未识别的选项
                show_usage(args[0], std::string("ERROR: unrecognized option " + std::string(args[curr])).c_str()); // 显示错误信息
                exit(1);                                                                                           // 退出程序
            }
        }

        // 解析位置参数
        if (listen)
        {                                          // 如果是服务器模式
            c_filt.source = {"0", args[curr + 1]}; // 设置源地址和端口
            if (c_filt.source.port() == 0)
            {                                                                             // 检查端口是否为零
                show_usage(args[0], "ERROR: listen port cannot be zero in server mode."); // 显示错误信息
                exit(1);                                                                  // 退出程序
            }
        }
        else
        {                                                      // 如果是客户端模式
            c_filt.destination = {args[curr], args[curr + 1]}; // 设置目标地址和端口
            c_filt.source = {source_address, source_port};     // 设置源地址和端口
        }

        return std::make_tuple(c_fsm, c_filt, listen, tundev); // 返回配置元组
    }
} // namespace

int main(int argc, char **argv)
{
    try
    {
        if (argc <= 0)
        {
            std::abort(); // 如果 argc <= 0，终止程序
        }

        auto args = std::span(argv, argc); // 使用 span 处理命令行参数

        if (argc < 3)
        {                                                                       // 检查参数数量
            show_usage(args.front(), "ERROR: required arguments are missing."); // 显示用法和错误信息
            return EXIT_FAILURE;                                                // 返回失败状态
        }

        // 获取配置
        auto [c_fsm, c_filt, listen, tun_dev_name] = get_config(args);
        // 创建 TCP socket
        LossyTCPOverIPv4MinnowSocket tcp_socket(LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>(
            TCPOverIPv4OverTunFdAdapter(TunFD(tun_dev_name == nullptr ? TUN_DFLT : tun_dev_name))));

        if (listen)
        {                                                // 如果是服务器模式
            tcp_socket.listen_and_accept(c_fsm, c_filt); // 监听并接受连接
        }
        else
        {                                      // 如果是客户端模式
            tcp_socket.connect(c_fsm, c_filt); // 连接到服务器
        }

        // 进行双向流复制
        bidirectional_stream_copy(tcp_socket, tcp_socket.peer_address().to_string());
        tcp_socket.wait_until_closed(); // 等待连接关闭
    }
    catch (const std::exception &e)
    {                                                        // 捕获异常
        std::cerr << "Exception: " << e.what() << std::endl; // 输出异常信息
        return EXIT_FAILURE;                                 // 返回失败状态
    }

    return EXIT_SUCCESS; // 返回成功状态
}
