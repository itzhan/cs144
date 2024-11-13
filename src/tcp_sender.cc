#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return window_.sequence_numbers_in_flight;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // 若已经建立起链接,且没有可读数据,且FIN已经被发送,则直接结束
  if ( ( input_.reader().bytes_buffered() == 0 ) && !( seqno_ == isn_ ) && window_.FIN_sent )
    return;

  while ( true ) {
    // 若还没有链接,发送SYN建立起链接
    TCPSenderMessage segment;
    if ( seqno_ == isn_ ) {
      segment.SYN = true;
    }
    // 获取可写入的长度
    uint64_t window_size = ( window_.right - 0 ) - ( window_.left - 0 );
    uint64_t occupied_size = window_.sequence_numbers_in_flight;
    uint64_t available_window = window_size > occupied_size ? window_size - occupied_size : 0;

    // 如果可用空间为0,则假装他还有1
    if ( window_size == 0 ) {
      if ( window_.sequence_numbers_in_flight > 0 )
        break;
      available_window = 1;
    }

    uint64_t writer_length = min( TCPConfig::MAX_PAYLOAD_SIZE, available_window );
    // 写入payload
    read( input_.reader(), writer_length - segment.SYN, segment.payload );
    // 若写入流已经关闭,且可用值大于1
    if ( input_.writer().is_closed() && !window_.FIN_sent && ( reader().bytes_buffered() == 0 )
         && ( available_window - segment.payload.size() ) >= 1 ) {
      segment.FIN = true;
      window_.FIN_sent = true;
    }
    // 设置RST信号
    segment.RST = reader().has_error();
    // 无数据且无信号,直接结束
    if ( segment.sequence_length() == 0 ) {
      break;
    }

    // 设置并更新seqno
    segment.seqno = seqno_;
    seqno_ = seqno_ + segment.sequence_length();
    window_.sequence_numbers_in_flight += segment.sequence_length();
    window_.in_flight_package.push( segment );
    // 发送
    transmit( segment );

    // 如果窗口为0,则发送一个后直接结束
    if ( window_size == 0 ) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return TCPSenderMessage { .seqno = seqno_ , .RST = reader().has_error()};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // 出现了error
  if (!msg.ackno.has_value() && msg.window_size == 0)
    input_.set_error();
  // sender需要的数据序列超过了我们目前的最大,则直接忽略
  if ( msg.ackno > seqno_ )
    return;

  // 只有存在ackno的时候才进行调整
  if ( !msg.ackno.has_value() && msg.window_size ) {
    window_.right = window_.left + msg.window_size;
    return;
  }
  // 当队列不为空时处理ACK
  while ( !window_.in_flight_package.empty() ) {
    auto first_item = window_.in_flight_package.front();
    // 只有当ACK号大于当前包右边界时才推进
    if ( msg.ackno.value() >= first_item.seqno + first_item.sequence_length() ) {
      // 更新tick以及重发次数
      current_RTO_ms_ = initial_RTO_ms_;
      consecutive_retransmissions_ = 0;
      timer_elapsed_ = 0;
      // 更新窗口状态
      window_.sequence_numbers_in_flight -= first_item.sequence_length();
      window_.in_flight_package.pop();
    } else {
      break;
    }
  }

  // 更新window右边界
  window_.right = window_.left + msg.window_size;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // 增加流逝时间
  timer_elapsed_ += ms_since_last_tick;

  // 超时出现
  if ( timer_elapsed_ >= current_RTO_ms_ ) {
    if ( !window_.in_flight_package.empty() ) {
      transmit( window_.in_flight_package.front() );
      if ((window_.left == window_.right)){
        current_RTO_ms_ += initial_RTO_ms_;
      }
      else {
        current_RTO_ms_ *= 2;
      }
      ++consecutive_retransmissions_;
    }

    if (!(window_.left == window_.right))
      timer_elapsed_ = 0;
  }
}