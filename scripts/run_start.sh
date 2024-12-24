#!/bin/bash

# 日志记录保存路径
log_file="/var/log/package_install.log"

# 检查日志文件路径是否可写
if [[ ! -w "$(dirname "$log_file")" ]]
then
    echo "Log directory is not writable. Exiting."
    exit 1
fi

function log()
{
    local log_level="$1"
    local message="$2"
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] [$log_level] - $message" | tee -a "$log_file"
}

function precheck_os_env()
{
    # 检查操作系统
    local current_os=$(uname -s)

    # 检查 Ubuntu 版本
    if [[ "$current_os" == "Linux" && -f "/etc/os-release" ]]
    then
        source /etc/os-release
        if [[ "$NAME" == "Ubuntu" ]]
        then
            # 使用 awk 比较版本号
            if awk -v ver="${VERSION_ID}" 'BEGIN { exit !(ver > 23.10) }'
            then
                log "info" "Current Ubuntu version is ${VERSION_ID}."
                return 0
            else
                log "warn" "Please ensure that you are using Ubuntu 23.10 or a later version."
                return 1
            fi
        else
            log "error" "Current operating system isn't Ubuntu."
            return 1
        fi
    else
        log "error" "Current operating system isn't Linux or /etc/os-release does not exist."
        return 1
    fi
}

function install_necessary_pkg()
{
    if sudo apt update
    then
        log "info" "Package lists updated successfully."
    else
        log "error" "Failed to update package lists. Please check the error messages."
        return 1
    fi

    # 检查并安装软件包
    packages=(git cmake gdb build-essential clang clang-tidy clang-format gcc-doc pkg-config glibc-doc tcpdump tshark)
    for package in "${packages[@]}"
    do
        if dpkg-query -W -f='${Status}' "$package" 2>/dev/null | grep -q "install ok installed"
        then
            log "info" "$package is already installed. Skipping."
        else
            log "info" "Installing $package..."
            if sudo apt-get install -y "$package"
            then
                log "info" "$package installed successfully."
            else
                log "error" "Installation of $package failed. Please check the error messages."
                return 1
            fi
        fi
    done
    log "info" "All required software packages are installed."
    return 0
}

function cmake_test_conditions()
{
    # 在当前目录下创建一个名为build的子目录
    cmake -S . -B build
    if [[ $? -ne 0 ]]
    then
        log "error" "cmake mkdir build failed."
        return 1
    fi

    # 编译并构建整个项目
    cmake --build build
    if [[ $? -ne 0 ]]
    then
        log "error" "cmake compile build failed."
        return 1
    fi

    # 启动一个名为tun144的TUN设备，并为其配置网络设置，使其能够在网络上发送和接收数据
    chmod +x ./scripts/tun.sh
    bash -x ./scripts/tun.sh start 144

    cmake --build build --target check_webget
    if [[ $? -ne 0 ]]
    then
        log "error" "test check_webget failed."
        return 1
    fi

    # 基于CS144测试用例，4和7不支持
    local test_checkers=(0 1 2 3 5 6)
    for idx in "${test_checkers[@]}"
    do
        cmake --build build --target "check${idx}"
        if [[ $? -ne 0 ]]
        then
            log "error" "test check${idx} failed."
            return 1
        else
            log "info" "test check${idx} successfully."
        fi
    done

    # 测试当前byte_stream和reassembler的传输速度
    cmake --build build --target speed
    if [[ $? -ne 0 ]]
    then
        log "error" "test speed failed."
        return 1
    fi
    return 0
}

function main()
{
    # 检查OS是否满足条件
    precheck_os_env
    if [[ $? -ne 0 ]]
    then
        log "error" "precheck_os_env failed."
        return 1
    fi

    # 安装环境所需补丁包
    install_necessary_pkg
    if [[ $? -ne 0 ]]
    then
        log "error" "install_necessary_pkg failed."
        return 1
    fi

    # cmake生成build并进行测试
    cmake_test_conditions
    if [[ $? -ne 0 ]]
    then
        log "error" "cmake_test_conditions failed."
        return 1
    else
        log "info" "cmake_test_conditions successfully."
    fi
    return 0
}

main
exit $?
