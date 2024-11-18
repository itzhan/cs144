#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // 寻找合适的位置插入node
  auto node = root;
  for ( uint32_t i = 31; i >= 32 - prefix_length; --i ) {
    uint32_t bit = ( route_prefix >> i ) & 1;
    if ( bit == 0 ) {
      if ( !node->left ) {
        node->left = std::make_shared<TreeNode>();
      }
      node = node->left;
    } else {
      if ( !node->right ) {
        node->right = std::make_shared<TreeNode>();
      }
      node = node->right;
    }
  }
  // 填充node值
  node->next_hop = next_hop;
  node->interface_num = interface_num;
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here.
  for ( auto& network : _interfaces ) {
    auto& data_receive = network->datagrams_received();

    while ( !data_receive.empty() ) {
      InternetDatagram datagram = data_receive.front();
      data_receive.pop();
      // 如果超过了TTL,则直接drop
      if ( datagram.header.ttl <= 1 ) {
        continue;
      }
      datagram.header.ttl--;

      uint32_t destination_ip = datagram.header.dst;
      // 二叉树搜索
      auto current = root;
      std::shared_ptr<TreeNode> result = nullptr;
      for ( uint32_t i = 31; i >= 0; --i ) {
        if ( !current )
          break;
        // 若当前节点存在,则记录(最长前缀匹配)
        if ( current->interface_num != std::numeric_limits<size_t>::max() ) {
          result = current;
        }
        uint32_t bit = ( destination_ip >> i ) & 1;
        current = ( bit == 0 ) ? current->left : current->right;
      }
      // 找到对应的route,若next_hop不存在 则直接drop,
      if ( result ) {
        Address next_address
          = result->next_hop.has_value() ? result->next_hop.value() : Address::from_ipv4_numeric( destination_ip );
        _interfaces[result->interface_num]->send_datagram( datagram, next_address );
      }
    }
  }
}
