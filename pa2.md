Programming Assignment 2 Writeup
====================

My name: [ZIXUAN ZENG]

My UTORID : [1008533419]

I would like to credit/thank these classmates for their help: ['Anonymous Scale' from piazza post @236]

This programming assignment took me about [4] hours to do.

Program Structure and Design of the Router:\
[New struct: (RoutingTableEntry), which contains information of an added route.\
New variable: (routing_table_) for the routing table, which contains a vector of (RoutingTableEntry)s.\
Function: (add_route()) simply instantiates a (RoutingTableEntry) and stores relevant information, then push into (routing_table_).\
Function: (route()) iterates trough all interfaces of the current router, then for each interface iterates through all optional datagrams. Then it routes datagram via LongestPrefixMatch, drops according to handout instructions, determines next hop, then send the datagram.
]

Implementation Challenges:
[None.]

Remaining Bugs:
[None. All public tests passed with Total Test time (real) = 9.71 sec.]

- Optional: I had unexpected difficulty with: [None.]

- Optional: I think you could make this lab better by: [It would be better to mention updating datagram by recomputing checksum after updating TTL.]

- Optional: I was surprised by: [None.]

- Optional: I'm not sure about: [None.]
