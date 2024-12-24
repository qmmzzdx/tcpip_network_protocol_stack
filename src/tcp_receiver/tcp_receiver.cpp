#include "tcp_receiver.h"

// TCPReceiver 类用于处理接收到的 TCP 消息并管理 TCP 流的接收状态。
void TCPReceiver::receive(TCPSenderMessage message)
{
    // 检查 writer 是否有错误，如果有错误则输出错误信息并返回。
    if (writer().has_error())
    {
        std::cerr << "tcp writer is error\n";
        return;
    }

    // 如果接收到的消息包含 RST 标志，则设置 reader 错误状态并返回。
    if (message.RST)
    {
        reader().set_error();
        return;
    }

    // 如果初始序列号（ISN）尚未设置，则检查消息是否包含 SYN 标志。
    if (!isn_.has_value())
    {
        // 如果消息不包含 SYN 标志，则输出错误信息并返回。
        if (!message.SYN)
        {
            std::cerr << "message syn is invalid.\n";
            return;
        }
        // 设置初始序列号为接收到的消息的序列号。
        isn_ = message.seqno;
    }

    // 计算当前的检查点（ckpt），即已经推送的字节数加 1。
    uint64_t ckpt = writer().bytes_pushed() + 1;

    // 计算消息的绝对序列号。
    uint64_t absseq = message.seqno.unwrap(isn_.value(), ckpt);

    // 计算流的索引，如果消息包含 SYN 标志，则索引为 0，否则为绝对序列号减 1。
    uint64_t stream_idx = message.SYN ? 0 : absseq - 1;

    // 将消息的有效载荷插入到流重组器中。
    reassembler_.insert(stream_idx, std::move(message.payload), message.FIN);
}

// 生成并返回一个 TCPReceiverMessage，表示接收方的状态。
TCPReceiverMessage TCPReceiver::send() const
{
    TCPReceiverMessage res;

    // 如果初始序列号（ISN）已设置，则计算应答序列号（ackno）。
    if (isn_.has_value())
    {
        // 计算绝对序列号，考虑到已经推送的字节数和流是否关闭。
        uint64_t absseq = writer().bytes_pushed() + 1 + writer().is_closed();
        // 将绝对序列号封装为相对序列号。
        res.ackno = Wrap32::wrap(absseq, isn_.value());
    }

    // 设置窗口大小为 writer 的可用容量和 UINT16_MAX 的较小值。
    res.window_size = static_cast<uint16_t>(std::min(writer().available_capacity(), static_cast<uint64_t>(UINT16_MAX)));

    // 设置 RST 标志为 writer 的错误状态。
    res.RST = writer().has_error();

    return res;
}
