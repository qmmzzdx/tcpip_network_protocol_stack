#ifndef BIDIRECTIONAL_STREAM_COPY_H
#define BIDIRECTIONAL_STREAM_COPY_H

#include "socket.h"
/*
 * 将套接字的输入/输出复制到标准输入/输出，直到完成
 * 该函数实现了在给定的 Socket 对象和标准输入/输出之间的双向数据流复制。
 * 它会持续读取来自套接字的数据并将其写入标准输出，同时也会从标准输入读取数据并写入套接字。
 */
void bidirectional_stream_copy(Socket &socket, std::string_view peer_name);

#endif
