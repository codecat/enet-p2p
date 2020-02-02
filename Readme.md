# ENet P2P example
This is an example for [ENet](http://enet.bespin.org/) that lets you connect 2 peers to each other that are both behind a NAT, using NAT hole punching and an intermediate server.

## The source
The source is merely an example, so I don't provide any means to build it. Although it should build and run okay, given you change `ServerHostName` in the client file to point to your own server.

* Server code: [p2ptest_server.cpp](p2ptest_server.cpp)
* Client code: [p2ptest_client.cpp](p2ptest_client.cpp)

## How it works
1. The server needs to be running on some publicly available (non-NAT) server.
2. Each client connects to the server via `enet_host_connect`.
3. Server sends full peer addresses of all connected peers to all other peers.
4. Each peer connects directly using `enet_host_connect` to the same address reported by the server.
5. Once 2 peers both connect to each other, the connection will be established and you may start sending packets!
