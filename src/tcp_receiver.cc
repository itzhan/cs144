#include "tcp_receiver.hh"
#include <cmath>
using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // 如果遇到了error,则设置error
  if ( message.RST ) {
    is_connect = false;
    reader().set_error();
    return;
  }

  if ( message.SYN && !is_connect) {
    is_connect = true;
    ISN = message.seqno;
    ack_number = 1;
  } else if (!is_connect) {
    // 如果还没有建立起链接,直接结束
      return;
  }

  uint64_t first_index = message.seqno.unwrap( ISN, ack_number );
  if (message.SYN) {
    first_index = 0;  // SYN包的数据从0开始
  } else {
    first_index = first_index - 1;  // 非SYN包需要减1
  }

  this->reassembler_.insert( first_index, message.payload, message.FIN );
  ack_number = reassembler_.writer().bytes_pushed() + 1;
  if (reassembler_.writer().is_closed()){
    ack_number += 1;
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage message;
  if ( ack_number != 0 ) {
    message.ackno = Wrap32::wrap( ack_number, ISN );
  }
  message.RST = reader().has_error();
  message.window_size = reassembler_.writer().available_capacity() < UINT16_MAX ? reassembler_.writer().available_capacity() : UINT16_MAX;
  return message;
}
