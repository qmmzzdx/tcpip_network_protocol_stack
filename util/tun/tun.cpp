#include <cstring>        // 引入 C 字符串处理函数
#include <fcntl.h>        // 引入文件控制定义
#include <linux/if.h>     // 引入网络接口相关定义
#include <linux/if_tun.h> // 引入 TUN/TAP 设备相关定义
#include <sys/ioctl.h>    // 引入 ioctl 函数的定义
#include "tun.h"          // 引入自定义的 TUN/TAP 设备类头文件
#include "exception.h"    // 引入自定义异常处理类头文件

// 定义 TUN/TAP 设备的路径
static constexpr const char *CLONEDEV = "/dev/net/tun";

// devname 是 TUN 或 TAP 设备的名称，在创建时指定。
// is_tun 如果为 `true`，则表示 TUN 设备（期望 IP 数据报）；如果为 `false`，则表示 TAP 设备（期望以太网帧）。
// 要创建 TUN 设备，在调用此函数之前以 root 身份运行：ip tuntap add mode tun user `username` name `devname`

// TunTapFD 类的构造函数
TunTapFD::TunTapFD(const std::string &devname, const bool is_tun)
    : FileDescriptor(::CheckSystemCall("open", open(CLONEDEV, O_RDWR | O_CLOEXEC))) // 打开 TUN/TAP 设备
{
    struct ifreq tun_req{}; // 创建 ifreq 结构体，用于请求 TUN/TAP 设备的配置

    // 设置设备标志，IFF_TUN 表示 TUN 设备，IFF_TAP 表示 TAP 设备，IFF_NO_PI 表示不提供数据包信息
    tun_req.ifr_flags = static_cast<int16_t>((is_tun ? IFF_TUN : IFF_TAP) | IFF_NO_PI);

    // 将设备名称复制到 ifr_name 中，确保以 null 结尾
    std::strncpy(static_cast<char *>(tun_req.ifr_name), devname.data(), IFNAMSIZ - 1); // 复制设备名称
    tun_req.ifr_name[IFNAMSIZ - 1] = '\0';                                             // 确保字符串以 null 结尾

    // 通过 ioctl 调用设置 TUN/TAP 设备的配置
    CheckSystemCall("ioctl", ioctl(fd_num(), TUNSETIFF, static_cast<void *>(&tun_req)));
}
