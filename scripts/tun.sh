#!/bin/bash

TUN_IP_PREFIX=169.254

function show_usage()
{
    echo "Usage: $0 <start | stop | restart | check> [tunnum ...]"
    exit 1
}

function start_tun()
{
    # 获取 TUN 设备编号，并构造 TUN 设备名称
    local TUNNUM="$1" TUNDEV="tun$1"

    # 创建一个新的 TUN 设备，指定用户为当前执行脚本的用户
    ip tuntap add mode tun user "${SUDO_USER}" name "${TUNDEV}"

    # 为 TUN 设备分配一个 IP 地址，格式为 169.254.<TUNNUM>.1/24
    ip addr add "${TUN_IP_PREFIX}.${TUNNUM}.1/24" dev "${TUNDEV}"

    # 将 TUN 设备设置为“up”状态，使其可用
    ip link set dev "${TUNDEV}" up

    # 设置路由，使得流量通过该 TUN 设备，rto_min 设置为 10ms
    ip route change "${TUN_IP_PREFIX}.${TUNNUM}.0/24" dev "${TUNDEV}" rto_min 10ms

    # 仅对来自 CS144 网络设备的流量应用 NAT（伪装）
    # 在 PREROUTING 链中添加规则，将源地址为 ${TUN_IP_PREFIX}.${TUNNUM}.0/24 的流量标记
    iptables -t nat -A PREROUTING -s ${TUN_IP_PREFIX}.${TUNNUM}.0/24 -j CONNMARK --set-mark ${TUNNUM}

    # 在 POSTROUTING 链中添加规则，伪装标记为 ${TUNNUM} 的流量
    iptables -t nat -A POSTROUTING -j MASQUERADE -m connmark --mark ${TUNNUM}

    # 启用 IP 转发，以允许流量在接口之间转发
    echo 1 > /proc/sys/net/ipv4/ip_forward
}

function stop_tun()
{
    # 构造 TUN 设备的名称，例如 tun0
    local TUNDEV="tun$1"

    # 从 NAT 表的 PREROUTING 链中删除与指定 TUN 网络相关的连接标记规则
    iptables -t nat -D PREROUTING -s ${TUN_IP_PREFIX}.${1}.0/24 -j CONNMARK --set-mark ${1}

    # 从 NAT 表的 POSTROUTING 链中删除与指定连接标记相关的伪装规则
    iptables -t nat -D POSTROUTING -j MASQUERADE -m connmark --mark ${1}

    # 删除指定的 TUN 设备
    ip tuntap del mode tun name "$TUNDEV"
}

function start_all()
{
    # 循环处理所有传入的参数，直到没有参数为止
    while [ ! -z "$1" ]
    do
        # 将当前参数赋值给 INTF 变量
        local INTF="$1"
        # 移除当前参数，准备处理下一个参数
        shift
        # 调用 start_tun 函数，启动指定的 TUN 设备
        start_tun "$INTF"
    done
}

function stop_all()
{
    # 循环处理所有传入的参数，直到没有参数为止
    while [ ! -z "$1" ]
    do
        # 将当前参数赋值给 INTF 变量
        local INTF="$1"
        # 移除当前参数，准备处理下一个参数
        shift
        # 调用 stop_tun 函数，停止指定的 TUN 设备
        stop_tun "$INTF"
    done
}

function restart_all()
{
    # 调用 stop_all 函数，停止所有指定的 TUN 设备
    stop_all "$@"
    
    # 调用 start_all 函数，启动所有指定的 TUN 设备
    start_all "$@"
}

function check_tun()
{
    # 检查传入的参数数量是否为 1，如果不是，则输出错误信息并退出
    [ "$#" != 1 ] && { echo "bad params in check_tun"; exit 1; }
    
    # 构造 TUN 设备的名称，例如 tun0
    local TUNDEV="tun${1}"
    
    # 确保 TUN 设备是健康的：设备处于活动状态，IP 转发已启用，并且 iptables 已配置
    # 检查 TUN 设备是否存在，如果不存在则返回 1
    ip link show ${TUNDEV} &>/dev/null || return 1
    
    # 检查 IP 转发是否已启用，如果未启用则返回 2
    [ "$(cat /proc/sys/net/ipv4/ip_forward)" = "1" ] || return 2
}

function check_sudo()
{
    # 检查当前用户是否为 root 用户
    if [ "$SUDO_USER" = "root" ]
    then
        # 如果是 root 用户，输出提示信息并退出
        echo "please execute this script as a regular user, not as root"
        exit 1
    fi
    
    # 检查是否通过 sudo 执行该脚本
    if [ -z "$SUDO_USER" ]
    then
        # 如果没有通过 sudo 执行，则重新以 sudo 权限执行该脚本
        exec sudo $0 "$MODE" "$@"
    fi
}

# 检查传入的参数
if [ -z "$1" ] || ([ "$1" != "start" ] && [ "$1" != "stop" ] && [ "$1" != "restart" ] && [ "$1" != "check" ])
then
    show_usage
fi

MODE="$1"  # 将第一个参数赋值给 MODE 变量
shift  # 移除第一个参数，后续参数将被处理

# 设置默认参数
if [ $# -eq 0 ]
then
    set -- 144 145  # 如果没有其他参数，设置默认值为 144 和 145
fi

# 在尝试使用 sudo 之前执行 'check'
# - 类似于 start，但如果一切正常则成功退出
if [ "$MODE" = "check" ]
then
    declare -a INTFS  # 声明一个数组用于存储接口
    MODE="start"
    while [ ! -z "$1" ]
    do
        INTF="$1"
        shift
        check_tun ${INTF}  # 检查接口的状态
        if [ $? -eq 0 ]
        then
            continue
        fi

        if [ $? -gt 1 ]  # 如果返回值大于 1，表示需要重启
        then
            MODE="restart"  # 将模式设置为 restart
        fi
        INTFS+=($INTF)  # 将当前接口添加到 INTFS 数组中
    done

    set -- "${INTFS[@]}"  # 将需要处理的接口设置为参数
    if [ $# -eq 0 ]
    then
        exit 0
    fi
    echo -e "[$0] Bringing up tunnels ${INTFS[@]}:"  # 输出正在启动的接口信息
fi

# 检查是否需要 sudo 权限
check_sudo "$@"

# 启动、停止或重启所有接口
eval "${MODE}_all" "$@"
