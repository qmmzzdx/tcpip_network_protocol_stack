#### 从头开始设计和实现一个简化的基于TCP/IP的网络协议栈(基于CS144改编和优化)

简化的基于TCP/IP的网络协议栈包括以下几个部分：
- 数据链路层：实现基本的帧格式和传输机制，处理数据的封装和解封装
- 网络层：实现 IP 协议，包括地址解析、路由选择和数据包转发
- 传输层：实现 TCP 或 UDP 协议，处理连接管理、流量控制和错误检测
- 应用层：实现简单的应用层协议(HTTP)，以便在服务器端和客户端进行数据传输

#### 项目运行流程
运行script文件夹下的run_start.sh可以一键安装所需的软件包和本地编译测试，并在/var/log/package_install.log下观察执行日志
- Linux> chmod +x ./script/run_start.sh
- Terminal1> bash -x ./script/run_start.sh
- Terminal2> tail -f /var/log/package_install.log
