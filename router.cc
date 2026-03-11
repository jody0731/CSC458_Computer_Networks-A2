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

  // (void)route_prefix;
  // (void)prefix_length;
  // (void)next_hop;
  // (void)interface_num;

  // create RoutingTableEntry for the route
  RoutingTableEntry entry;
  entry.route_prefix = route_prefix;
  entry.prefix_length = prefix_length;
  entry.next_hop = next_hop;
  entry.interface_num = interface_num;
  // push into routing_table_ (move adapted from router.hh)
  routing_table_.push_back( std::move( entry ) );
}

void Router::route() {
  // iterate over network interfaces
  for (auto& interface : interfaces_) {

    // iterate over all datagrams waiting on this interface
    while (auto datagram_opt = interface.maybe_receive()) {
        auto datagram = datagram_opt.value();
        // destination (uint32_t)
        uint32_t dest = datagram.header.dst;
        
        // initialize best matched route
        RoutingTableEntry* best_route = nullptr;
        // find the best route via longest prefix match
        for (auto& route : routing_table_) {
            // special case: prefix_length 0
            if (route.prefix_length == 0) {
                if (best_route == nullptr)
                    best_route = &route;
            } else {
                // mask with the most significant (prefix_length) bits.
                uint32_t mask = 0xFFFFFFFF << (32 - route.prefix_length);
                if ((dest & mask) == route.route_prefix) {
                    if ((best_route == nullptr) || (route.prefix_length > best_route->prefix_length))
                        best_route = &route;
                }
            }
        }

        // drop packet if no matching route found
        if (best_route == nullptr)
            continue;
        // decrement TTL. drop the datagram if TTL == 0.
        if (datagram.header.ttl <= 1)
            continue;
        datagram.header.ttl--;
        // checksum. notified from piazza post @236.
        datagram.header.compute_checksum();

        // next hop
        std::optional<Address> next_hop_opt;
        if (best_route->next_hop.has_value()) {
            next_hop_opt = best_route->next_hop;
        } else {
            next_hop_opt = Address::from_ipv4_numeric(datagram.header.dst);
        }

        // sending datagram 
        interfaces_[ best_route->interface_num ].send_datagram( datagram, next_hop_opt.value() );
    }
  }
}
