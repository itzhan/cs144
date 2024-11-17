#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t next_hop_ip_address = next_hop.ipv4_numeric();
  EthernetHeader header;
  EthernetFrame frame;
  header.src = this->ethernet_address_;
  // 如果不存在,则将数据包压入queue并发送广播去找
  if ( ip_to_eth_.find( next_hop_ip_address ) == ip_to_eth_.end() ) {
    // 如果已经在5s内被发送,则直接返回
    if (pending_time_.find(next_hop_ip_address) != pending_time_.end()){
      return;
    }

    // 压入queue,接收到数据后进行处理
    datagrams_wait_output[next_hop_ip_address] = dgram;
    pending_time_[next_hop_ip_address] = 0;
    // 设置头信息
    header.dst = ETHERNET_BROADCAST;
    header.type = EthernetHeader::TYPE_ARP;
    frame.header = header;
    // 创建arp消息
    ARPMessage arp;
    arp.opcode = ARPMessage::OPCODE_REQUEST;
    arp.sender_ethernet_address = this->ethernet_address_;
    arp.sender_ip_address = this->ip_address_.ipv4_numeric();
    arp.target_ip_address = next_hop_ip_address;
    // 进行序列化
    Serializer serialize;
    arp.serialize( serialize );
    // 将值传递
    frame.payload = serialize.output();
  }
  // 若存在,则直接发送
  else {
    header.dst = ip_to_eth_[next_hop_ip_address];
    header.type = EthernetHeader::TYPE_IPv4;
    frame.header = header;

    Serializer serializer;
    dgram.serialize( serializer );
    frame.payload = serializer.output();
  }

  transmit( frame );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // 若目标不是广播,且目标地址不等于本机,则直接结束
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != this->ethernet_address_ ) {
    return;
  }

  Parser parse( frame.payload );
  // 如果为ip包,则push到queue中
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram gram;
    gram.parse( parse );
    datagrams_received_.push( gram );
  }
  // 如果为ARP包
  else if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage arp;
    arp.parse( parse );
    // 如果收到了请求,且本设备的ip地址等于请求的
    if ( arp.opcode == ARPMessage::OPCODE_REQUEST
         && ( arp.target_ip_address == this->ip_address_.ipv4_numeric() ) ) {
      // 建立起映射
      ip_to_eth_[arp.sender_ip_address] = arp.sender_ethernet_address;
      store_time_[arp.sender_ip_address] = 0;
      // 创建reply消息
      ARPMessage reply;
      reply.opcode = ARPMessage::OPCODE_REPLY;
      reply.sender_ethernet_address = this->ethernet_address_;
      reply.sender_ip_address = this->ip_address_.ipv4_numeric();
      reply.target_ethernet_address = arp.sender_ethernet_address;
      reply.target_ip_address = arp.sender_ip_address;
      Serializer serialize;
      reply.serialize( serialize );
      // 创建Ethernet消息
      EthernetFrame ethernet;
      ethernet.payload = serialize.output();
      ethernet.header.type = EthernetHeader::TYPE_ARP;
      ethernet.header.src = this->ethernet_address_;
      ethernet.header.dst = arp.sender_ethernet_address;

      transmit( ethernet );
    } else if ( arp.opcode == ARPMessage::OPCODE_REPLY ) {
      ip_to_eth_[arp.sender_ip_address] = arp.sender_ethernet_address;
      store_time_[arp.sender_ip_address] = 0;
      // 遍历待发送的数据,并找到所有等于该ip地址的进行发送
      for ( auto pair : datagrams_wait_output ) {
        if ( pair.first == arp.sender_ip_address )
          send_datagram( pair.second, Address::from_ipv4_numeric( pair.first ) );
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // 遍历ip --> MAC 地址映射表,删除过时项目
  auto pair = store_time_.begin();
  while ( pair != store_time_.end() ) {
    pair->second += ms_since_last_tick;
    if ( pair->second >= 30000 ) {
      ip_to_eth_.erase( pair->first );
      pair = store_time_.erase( pair );
    } else {
      ++pair;
    }
  }

  // 遍历pending_time表,删除过时申请
  auto pair2 = pending_time_.begin();
  while ( pair2 != pending_time_.end() ) {
    pair2->second += ms_since_last_tick;
    // expire
    if ( pair2->second >= 5000 ) {
      pair2 = pending_time_.erase( pair2 );
    } else {
      ++pair2;
    }
  }
}
