#ifndef TUN_H
#define TUN_H

#include <string>             // 引入字符串类
#include "file_descriptor.h"  // 引入自定义的文件描述符类

// TunTapFD 类用于表示 TUN/TAP 设备的文件描述符
class TunTapFD : public FileDescriptor
{
public:
    // 构造函数，接受设备名称和设备类型（TUN 或 TAP）
    explicit TunTapFD(const std::string &devname, bool is_tun);
};

// TunFD 类表示 TUN 设备，继承自 TunTapFD
class TunFD : public TunTapFD
{
public:
    // 构造函数，默认将 is_tun 设置为 true，表示这是一个 TUN 设备
    explicit TunFD(const std::string &devname) : TunTapFD(devname, true) {}
};

// TapFD 类表示 TAP 设备，继承自 TunTapFD
class TapFD : public TunTapFD
{
public:
    // 构造函数，默认将 is_tun 设置为 false，表示这是一个 TAP 设备
    explicit TapFD(const std::string &devname) : TunTapFD(devname, false) {}
};

#endif // TUN_H
