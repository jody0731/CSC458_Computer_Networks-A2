#include "network_interface.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;


// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{

  cerr << "DEBUG: Network interface has Ethernet address ";
  cerr << to_string( ethernet_address_ );
  cerr << " and IP address ";
  cerr << ip_address.ip() << "\n";
}


// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // convert IP to uint32_t key
  uint32_t net_hop_key = next_hop.ipv4_numeric();

  // lookup IP address in ARP Table 
  if (_arp_table.find(net_hop_key) != _arp_table.end()) {
    // next_hop IP found
    // set frame
    EthernetFrame frame;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.header.src = ethernet_address_;
    frame.header.dst = _arp_table[net_hop_key].mac;
    frame.payload = serialize(dgram);
    // push frames
    _frames_ready.push(frame);
    
  } else {
    // next_hop IP not found
    // lookup ARP request
    if (_arp_request_table.find(net_hop_key) != _arp_request_table.end()) {
      // ARP entry found (requested within 5 seconds)
      _arp_request_table[net_hop_key].dgrams.push(dgram);

    } else {
      // ARP entry not found
      // set ARP entry
      ARPRequestEntry arp_req;
      arp_req.ttl = 5000;
      arp_req.dgrams.push(dgram);
      _arp_request_table[net_hop_key] = arp_req;
      // set ARP request
      ARPMessage arp_req_msg;
      arp_req_msg.opcode = ARPMessage::OPCODE_REQUEST;
      arp_req_msg.sender_ethernet_address = ethernet_address_;
      arp_req_msg.sender_ip_address = ip_address_.ipv4_numeric();
      arp_req_msg.target_ip_address = net_hop_key;
      // set ARP frame
      EthernetFrame arp_frame;
      arp_frame.header.src = ethernet_address_;
      arp_frame.header.dst = ETHERNET_BROADCAST; 
      arp_frame.header.type = EthernetHeader::TYPE_ARP;
      arp_frame.payload = serialize(arp_req_msg);
      // push ARP frame
      _frames_ready.push(arp_frame);
    }

  }
}

// helper function: Handling both ARP types, and respond accordingly.=
void NetworkInterface::arp_msg_handler( const ARPMessage& arp_msg )
{
  // check ARP type
  if ((arp_msg.opcode == ARPMessage::OPCODE_REQUEST) &&
      (arp_msg.target_ip_address == ip_address_.ipv4_numeric())) {
    // type: request for this interface's (IP?)MAC // suspected typo in handout
    // set ARP reply
    ARPMessage arp_rep_msg;
    arp_rep_msg.opcode = ARPMessage::OPCODE_REPLY;
    arp_rep_msg.sender_ethernet_address = ethernet_address_;
    arp_rep_msg.sender_ip_address = ip_address_.ipv4_numeric();
    arp_rep_msg.target_ethernet_address = _arp_table[arp_msg.sender_ip_address].mac;
    arp_rep_msg.target_ip_address = arp_msg.sender_ip_address;
    // set ARP frame
    EthernetFrame arp_frame;
    arp_frame.header.src = ethernet_address_;
    arp_frame.header.dst = arp_msg.sender_ethernet_address; 
    arp_frame.header.type = EthernetHeader::TYPE_ARP;
    arp_frame.payload = serialize(arp_rep_msg);
    // push ARP frame
    _frames_ready.push(arp_frame);

  } 
  // else if (arp_msg.opcode == ARPMessage::OPCODE_REPLY) {}
  // Regardless of the ARP message type, find and send all queued frames from the sender.
  auto entry = _arp_request_table.find(arp_msg.sender_ip_address);
  if (entry != _arp_request_table.end()) {
    while (!entry->second.dgrams.empty()) {
      InternetDatagram pending = entry->second.dgrams.front();
      entry->second.dgrams.pop();
      EthernetFrame pending_frame;
      pending_frame.header.src = ethernet_address_;
      pending_frame.header.dst = arp_msg.sender_ethernet_address;
      pending_frame.header.type = EthernetHeader::TYPE_IPv4;
      pending_frame.payload = serialize(pending);
      _frames_ready.push(pending_frame);
    }
    // erase the ARP request entry
    _arp_request_table.erase(entry);
  }
}


// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // check destination MAC
  if ((frame.header.dst != ethernet_address_) &&
      (frame.header.dst != ETHERNET_BROADCAST)) {
    // not sent to this interface, disgard
    return {};
  }

  // check type
  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    // type IPv4
    InternetDatagram dgram;
    if (parse(dgram, frame.payload)) {
      // parse success
      return dgram;
    }

  } else if (frame.header.type == EthernetHeader::TYPE_ARP) {
    // type ARP
    ARPMessage arp_msg;
    if (parse(arp_msg, frame.payload)) {
      // parse success
      // update ARP cache
      if (_arp_table.find(arp_msg.sender_ip_address) != _arp_table.end()) {
        // found in ARP cache, update ttl
        _arp_table[arp_msg.sender_ip_address].ttl = 30000;
      } else{
        // not found in ARP cache, set ARP cache entry
        ARPCacheEntry arp_cache;
        arp_cache.mac = arp_msg.sender_ethernet_address;
        arp_cache.ttl = 30000;
        // map ip to entry
        _arp_table[arp_msg.sender_ip_address] = arp_cache;
      }
      
      // call (helper function) ARP message handler
      arp_msg_handler(arp_msg);
    }
  }

  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // loop over ARP cache entries
  for (auto entry = _arp_table.begin(); entry != _arp_table.end();) {
    if (entry->second.ttl < ms_since_last_tick) {
      // timed out, erase
      entry = _arp_table.erase(entry);
    } else {
      entry->second.ttl -= ms_since_last_tick;
      entry++;
    }
  }

  // loop over ARP request entries
  for (auto entry = _arp_request_table.begin(); entry != _arp_request_table.end();) {
    if (entry->second.ttl < ms_since_last_tick) {
      // timed out, erase
      entry = _arp_request_table.erase(entry);
    } else {
      entry->second.ttl -= ms_since_last_tick;
      entry++;
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (!_frames_ready.empty()) {
    // ready-to-send queue not enpty
    EthernetFrame frame = _frames_ready.front();
    _frames_ready.pop();
    return frame;
  }
  return {};
}
