#include <cstdlib>                     // 包含 C 标准库的头文件
#include <cstring>                     // 包含字符串处理的头文件
#include <iostream>                    // 包含输入输出流的头文件
#include <span>                        // C++20 的 span 头文件，用于处理数组视图
#include "bidirectional_stream_copy.h" // 引入自定义的头文件，包含双向流复制的函数声明

// 显示程序的使用说明
void show_usage(const char *argv0)
{
    std::cerr << "Usage: " << argv0 << " [-l] <host> <port>\n\n";
    std::cerr << "  -l specifies listen mode; <host>:<port> is the listening address." << std::endl;
}

int main(int argc, char **argv)
{
    try
    {
        if (argc <= 0)
        {
            std::abort(); // 如果 argc <= 0，终止程序
        }

        std::span<char *> args(argv, argc); // 使用 span 处理命令行参数
        bool server_mode = false;           // 标志变量，指示是否为服务器模式

        // 检查命令行参数的有效性
        // 如果参数少于 3 或者在服务器模式下参数少于 4，则显示用法并退出
        if (argc < 3 || ((server_mode = (std::strncmp("-l", args[1], 3) == 0)) && argc < 4))
        {
            show_usage(args[0]); // 显示用法
            return EXIT_FAILURE; // 返回失败状态
        }

        // 创建 socket 的 lambda 表达式
        auto socket = [&]
        {
            if (server_mode)
            {                                                                                                       // 如果是服务器模式
                TCPSocket listening_socket;                                                                         // 创建一个 TCP socket
                listening_socket.set_reuseaddr();                                                                   // 设置地址重用选项
                listening_socket.bind({args[2], args[3]});                                                          // 绑定到指定的地址和端口
                listening_socket.listen();                                                                          // 开始监听传入的连接
                std::cerr << "DEBUG: Listening for incoming connection...\n";                                       // 调试信息
                TCPSocket connected_socket = listening_socket.accept();                                             // 接受连接
                std::cerr << "DEBUG: New connection from " << connected_socket.peer_address().to_string() << ".\n"; // 显示连接信息
                return connected_socket;                                                                            // 返回连接的 socket
            }
            // 客户端模式
            TCPSocket connecting_socket;                                                                               // 创建一个 TCP socket
            const Address peer{args[1], args[2]};                                                                      // 创建目标地址
            std::cerr << "DEBUG: Connecting to " << peer.to_string() << "... ";                                        // 调试信息
            connecting_socket.connect(peer);                                                                           // 连接到目标地址
            std::cerr << "DEBUG: Successfully connected to " << connecting_socket.peer_address().to_string() << ".\n"; // 显示连接成功信息
            return connecting_socket;                                                                                  // 返回连接的 socket
        }();
        // 调用 bidirectional_stream_copy 函数进行双向数据流复制
        bidirectional_stream_copy(socket, socket.peer_address().to_string());
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
