#ifndef TCP_RECEIVER_H
#define TCP_RECEIVER_H

#include <algorithm>
#include "reassembler.h"
#include "tcp_receiver_message.h"
#include "tcp_sender_message.h"

/**
 * @class TCPReceiver
 * @brief 负责接收来自 TCPSender 的消息，并将其有效载荷插入到 Reassembler 中。
 *
 * TCPReceiver 类用于处理 TCP 数据接收的逻辑。它接收来自对等方的 TCPSenderMessage，
 * 并将这些消息的有效载荷插入到 Reassembler 中，以便正确地重组数据流。
 */
class TCPReceiver
{
public:
    /**
     * @brief 使用给定的 Reassembler 构造 TCPReceiver。
     * @param reassembler 用于重组数据流的 Reassembler 对象。
     */
    explicit TCPReceiver(Reassembler &&reassembler) : reassembler_(std::move(reassembler)) {}

    /**
     * @brief 接收来自 TCPSender 的消息，并将其有效载荷插入到 Reassembler 中。
     * @param message 来自 TCPSender 的消息。
     *
     * 该方法负责处理接收到的 TCPSenderMessage，并将其有效载荷插入到 Reassembler 中的正确流索引位置。
     */
    void receive(TCPSenderMessage message);

    /**
     * @brief 发送 TCPReceiverMessage 给对等方的 TCPSender。
     * @return 构造的 TCPReceiverMessage。
     *
     * 该方法用于生成并返回一个 TCPReceiverMessage，供对等方的 TCPSender 使用。
     */
    TCPReceiverMessage send() const;

    /**
     * @brief 获取 Reassembler 的常量引用。
     * @return Reassembler 的常量引用。
     */
    const Reassembler &reassembler() const { return reassembler_; }

    /**
     * @brief 获取 Reader 的引用。
     * @return Reader 的引用。
     *
     * 该方法返回 Reassembler 中的 Reader 的引用，用于读取重组后的数据流。
     */
    Reader &reader() { return reassembler_.reader(); }

    /**
     * @brief 获取 Reader 的常量引用。
     * @return Reader 的常量引用。
     */
    const Reader &reader() const { return reassembler_.reader(); }

    /**
     * @brief 获取 Writer 的常量引用。
     * @return Writer 的常量引用。
     *
     * 该方法返回 Reassembler 中的 Writer 的常量引用，用于写入数据流。
     */
    const Writer &writer() const { return reassembler_.writer(); }

private:
    Reassembler reassembler_;     ///< 用于重组数据流的 Reassembler 实例。
    std::optional<Wrap32> isn_{}; ///< 初始序列号（ISN），用于 TCP 序列号的处理。
};

#endif
