#include <iostream>
#include <span>
#include <string>
#include <cstdlib>
#include "socket.h"
#include "tcp_minnow_socket.h"

void get_URL(const std::string &host, const std::string &path)
{
    std::string url_res;      // 用于存储服务器响应的字符串
    TINYTCPSocket url_client; // 创建一个 TINYTCPSocket 对象，用于与服务器进行通信

    // 连接到指定主机的 HTTP 服务器，默认端口为 80
    url_client.connect(Address(host, "http"));

    // 发送 HTTP GET 请求
    url_client.write("GET " + path + " HTTP/1.1\r\n");
    url_client.write("Host: " + host + "\r\nConnection: close\r\n\r\n");

    // 从服务器读取响应，直到读取到 EOF
    while (!url_client.eof())
    {
        url_client.read(url_res); // 从套接字读取数据并存入 url_res
        std::cout << url_res;     // 将响应数据输出到控制台
    }
    url_client.wait_until_closed(); // 等待连接完全关闭
}

int main(int argc, char *argv[])
{
    try
    {
        // 检查命令行参数是否大于 0，防止访问 argv[0] 时发生错误
        if (argc <= 0)
        {
            std::abort(); // 严格检查: 如果 argc <= 0，不应访问 argv[0]
        }
        auto args = std::span(argv, argc); // 使用 span 封装命令行参数

        // 程序需要两个命令行参数: 主机名和 URL 路径部分
        // 如果没有提供这两个参数 (加上程序名，所以总共应为 3 个参数)，则打印用法信息
        if (argc != 3)
        {
            std::cerr << "Usage: " << args.front() << " HOST PATH\n";
            std::cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // 获取命令行参数
        const std::string host{args[1]}; // 获取第一个参数，即主机名
        const std::string path{args[2]}; // 获取第二个参数，即 URL 的路径部分
        get_URL(host, path);             // 调用 get_URL 函数发送请求并输出响应
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
