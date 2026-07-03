# CSC458 – Programming Assignment 2: IP Router & Network Interface

Individual assignment for University of Toronto's CSC458 (Computer Networks), implementing an IP router that performs longest-prefix-match forwarding on top of a network interface that handles Ethernet framing and ARP resolution. Built on the Stanford CS144 ("Sponge") TCP/IP stack framework.

## What's in this repo

Only the two modules written for this assignment are included here — the course-provided framework (headers like `address.hh`, `arp_message.hh`, `ethernet_frame.hh`, `ipv4_datagram.hh`, the CMake build, and the test suite) is not part of this repo, since it isn't student-authored. As a result this code **isn't buildable standalone**.

### `network_interface.cc` / `.hh`

Translates between IP datagrams and Ethernet frames on a single link:

- `send_datagram`: looks up the next hop's MAC address in the ARP cache. If unknown, queues the datagram and broadcasts an ARP request (cached IP→MAC mappings expire after 30s; a pending ARP request is not re-sent for 5s).
- `recv_frame`: for IPv4 frames addressed to this interface, returns the parsed datagram up the stack. For ARP requests/replies, learns the sender's IP→MAC mapping, answers requests targeted at this interface, and flushes any datagrams that were queued waiting on that resolution.
- `tick`: ages out expired ARP cache entries and pending ARP requests.

### `router.cc` / `.hh`

- `add_route`: appends a `RoutingTableEntry` (prefix, prefix length, optional next hop, outbound interface) to the routing table.
- `route`: for each interface, drains its queue of received datagrams and, for each one, finds the routing-table entry with the longest matching prefix. Drops the packet if no route matches or if TTL ≤ 1; otherwise decrements the TTL, recomputes the checksum, and sends it out the matched interface toward the next hop (or the datagram's own destination address, if the network is directly attached).

## Design notes

See [pa2.md](pa2.md) for the original assignment writeup (design rationale, time spent, and known issues).

## Author

Zixuan Zeng
