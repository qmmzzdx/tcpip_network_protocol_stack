#include "tcp_minnow_socket.h"

// 实例化 TCPMinnowSocket，使用 TCPOverIPv4OverTunFdAdapter 作为适配器
template class TCPMinnowSocket<TCPOverIPv4OverTunFdAdapter>;  
// 实例化 TCPMinnowSocket，使用 LossyFdAdapter 包装 TCPOverIPv4OverTunFdAdapter，表示可能丢失数据的适配器
template class TCPMinnowSocket<LossyFdAdapter<TCPOverIPv4OverTunFdAdapter>>;  
