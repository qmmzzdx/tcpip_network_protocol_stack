#include <iostream>                    // 引入输入输出流的头文件
#include <unistd.h>                    // 引入 UNIX 标准头文件，提供对 POSIX 操作系统 API 的访问
#include <algorithm>                   // 引入算法头文件，提供常用算法的定义
#include "byte_stream.h"               // 引入自定义的字节流类的头文件
#include "eventloop.h"                 // 引入事件循环类的头文件
#include "bidirectional_stream_copy.h" // 引入双向流复制函数的头文件

void bidirectional_stream_copy(Socket &socket, std::string_view peer_name)
{
    constexpr size_t buffer_size = 1048576; // 定义缓冲区大小为 1MB

    EventLoop _eventloop{};                // 创建事件循环对象
    FileDescriptor _input{STDIN_FILENO};   // 创建文件描述符对象，表示标准输入
    FileDescriptor _output{STDOUT_FILENO}; // 创建文件描述符对象，表示标准输出
    ByteStream _outbound{buffer_size};     // 创建字节流对象，用于存储从标准输入读取的数据
    ByteStream _inbound{buffer_size};      // 创建字节流对象，用于存储从套接字读取的数据
    bool _outbound_shutdown{false};        // 标记是否关闭了输出流
    bool _inbound_shutdown{false};         // 标记是否关闭了输入流

    socket.set_blocking(false);  // 将套接字设置为非阻塞模式
    _input.set_blocking(false);  // 将输入文件描述符设置为非阻塞模式
    _output.set_blocking(false); // 将输出文件描述符设置为非阻塞模式

    // 规则 1: 从标准输入读取数据到输出字节流
    _eventloop.add_rule(
        "read from stdin into outbound byte stream", // 规则名称
        _input,                                      // 监视的文件描述符
        Direction::In,                               // 方向为输入
        [&]
        {
            std::string data;                                     // 创建字符串用于存储读取的数据
            data.resize(_outbound.writer().available_capacity()); // 调整字符串大小以适应可用容量
            _input.read(data);                                    // 从标准输入读取数据
            _outbound.writer().push(std::move(data));             // 将读取的数据推入输出字节流
            if (_input.eof())
            {                               // 如果到达输入流的末尾
                _outbound.writer().close(); // 关闭输出字节流
            }
        },
        [&]
        {
            // 检查是否可以继续读取
            return !_outbound.has_error() && !_inbound.has_error() &&
                   (_outbound.writer().available_capacity() > 0) &&
                   !_outbound.writer().is_closed();
        },
        [&]
        { _outbound.writer().close(); }, // 关闭输出字节流
        [&]
        {
            std::cerr << "DEBUG: Outbound stream had error from source.\n"; // 输出调试信息
            _outbound.set_error();                                          // 设置输出字节流的错误状态
            _inbound.set_error();                                           // 设置输入字节流的错误状态
        });

    // 规则 2: 从输出字节流读取数据到套接字
    _eventloop.add_rule(
        "read from outbound byte stream into socket", // 规则名称
        socket,                                       // 监视的套接字
        Direction::Out,                               // 方向为输出
        [&]
        {
            if (_outbound.reader().bytes_buffered())
            {                                                                    // 如果输出字节流中有缓冲数据
                _outbound.reader().pop(socket.write(_outbound.reader().peek())); // 将数据写入套接字
            }
            if (_outbound.reader().is_finished())
            {                                                                             // 如果输出字节流已完成
                socket.shutdown(SHUT_WR);                                                 // 关闭套接字的写入端
                _outbound_shutdown = true;                                                // 设置输出流关闭标志
                std::cerr << "DEBUG: Outbound stream to " << peer_name << " finished.\n"; // 输出调试信息
            }
        },
        [&]
        {
            // 检查是否可以继续读取
            return _outbound.reader().bytes_buffered() ||
                   (_outbound.reader().is_finished() && !_outbound_shutdown);
        },
        [&]
        { _outbound.writer().close(); }, // 关闭输出字节流
        [&]
        {
            std::cerr << "DEBUG: Outbound stream had error from destination.\n"; // 输出调试信息
            _outbound.set_error();                                               // 设置输出字节流的错误状态
            _inbound.set_error();                                                // 设置输入字节流的错误状态
        });

    // 规则 3: 从套接字读取数据到输入字节流
    _eventloop.add_rule(
        "read from socket into inbound byte stream", // 规则名称
        socket,                                      // 监视的套接字
        Direction::In,                               // 方向为输入
        [&]
        {
            std::string data;                                    // 创建字符串用于存储读取的数据
            data.resize(_inbound.writer().available_capacity()); // 调整字符串大小以适应可用容量
            socket.read(data);                                   // 从套接字读取数据
            _inbound.writer().push(std::move(data));             // 将读取的数据推入输入字节流
            if (socket.eof())
            {                              // 如果到达套接字的末尾
                _inbound.writer().close(); // 关闭输入字节流
            }
        },
        [&]
        {
            // 检查是否可以继续读取
            return !_inbound.has_error() && !_outbound.has_error() &&
                   (_inbound.writer().available_capacity() > 0) &&
                   !_inbound.writer().is_closed();
        },
        [&]
        { _inbound.writer().close(); }, // 关闭输入字节流
        [&]
        {
            std::cerr << "DEBUG: Inbound stream had error from source.\n"; // 输出调试信息
            _outbound.set_error();                                         // 设置输出字节流的错误状态
            _inbound.set_error();                                          // 设置输入字节流的错误状态
        });

    // 规则 4: 从输入字节流读取数据到标准输出
    _eventloop.add_rule(
        "read from inbound byte stream into stdout", // 规则名称
        _output,                                     // 监视的文件描述符
        Direction::Out,                              // 方向为输出
        [&]
        {
            if (_inbound.reader().bytes_buffered())
            {                                                                   // 如果输入字节流中有缓冲数据
                _inbound.reader().pop(_output.write(_inbound.reader().peek())); // 将数据写入标准输出
            }
            if (_inbound.reader().is_finished())
            {                             // 如果输入字节流已完成
                _output.close();          // 关闭标准输出
                _inbound_shutdown = true; // 设置输入流关闭标志
                std::cerr << "DEBUG: Inbound stream from " << peer_name << " finished";
                std::cerr << (_inbound.has_error() ? " uncleanly.\n" : ".\n");
            }
        },
        [&]
        {
            // 检查是否可以继续读取
            return _inbound.reader().bytes_buffered() ||
                   (_inbound.reader().is_finished() && !_inbound_shutdown);
        },
        [&]
        { _inbound.writer().close(); }, // 关闭输入字节流
        [&]
        {
            std::cerr << "DEBUG: Inbound stream had error from destination.\n"; // 输出调试信息
            _outbound.set_error();                                              // 设置输出字节流的错误状态
            _inbound.set_error();                                               // 设置输入字节流的错误状态
        });

    // 循环直到完成
    while (true)
    {
        if (EventLoop::Result::Exit == _eventloop.wait_next_event(-1))
        {           // 等待下一个事件
            return; // 如果接收到退出信号，返回
        }
    }
}
