#include <cstdio>
#include <cstdint>
#include <cstring>

#include <vector>
#include <algorithm>

#include <enet/enet.h>

#include <stdio.h>

static std::vector<ENetPeer*> g_peers;

class QuickBuffer
{
private:
	char* m_buffer;
	size_t m_maxSize;
	size_t m_cursor;

public:
	QuickBuffer(size_t maxSize = 0x1000)
	{
		m_buffer = (char*)malloc(maxSize);
		m_maxSize = maxSize;
		m_cursor = 0;
	}

	~QuickBuffer()
	{
		free(m_buffer);
	}

	template<typename T>
	void Write(const T& value)
	{
		memcpy(m_buffer + m_cursor, &value, sizeof(T));
		m_cursor += sizeof(T);
	}

	void Reset()
	{
		m_cursor = 0;
	}

	const void* GetBuffer() { return m_buffer; }
	size_t GetSize() { return m_cursor; }
};

enum PeerCommand : uint8_t
{
	pc_None,
	pc_PeerList,
	pc_NewPeer,
};

int main()
{
	QuickBuffer b;

	enet_initialize();

	ENetAddress hostAddress;
	hostAddress.host = ENET_HOST_ANY;
	hostAddress.port = 7788;
	ENetHost* host = enet_host_create(&hostAddress, 100, 2, 0, 0);

	printf("Listening on port %d\n", (int)hostAddress.port);
	while (true) {
		ENetEvent evt;
		if (enet_host_service(host, &evt, 1000) > 0) {
			char ip[40];
			enet_address_get_host_ip(&evt.peer->address, ip, 40);

			if (evt.type == ENET_EVENT_TYPE_CONNECT) {
				printf("New connection from %s\n", ip);

				b.Reset();
				b.Write(pc_PeerList);
				b.Write((uint32_t)g_peers.size());
				for (auto peer : g_peers) {
					b.Write(peer->address.host);
					b.Write(peer->address.port);
				}
				ENetPacket* packetPeerList = enet_packet_create(b.GetBuffer(), b.GetSize(), ENET_PACKET_FLAG_RELIABLE);
				enet_peer_send(evt.peer, 0, packetPeerList);

				b.Reset();
				b.Write(pc_NewPeer);
				b.Write(evt.peer->address.host);
				b.Write(evt.peer->address.port);
				ENetPacket* packetNewPeer = enet_packet_create(b.GetBuffer(), b.GetSize(), ENET_PACKET_FLAG_RELIABLE);
				for (auto peer : g_peers) {
					enet_peer_send(peer, 0, packetNewPeer);
				}

				g_peers.emplace_back(evt.peer);

			} else if (evt.type == ENET_EVENT_TYPE_DISCONNECT) {
				printf("Connection closed from %s\n", ip);
				auto it = std::find(g_peers.begin(), g_peers.end(), evt.peer);
				g_peers.erase(it);

			} else if (evt.type == ENET_EVENT_TYPE_RECEIVE) {
				printf("Data received! %d bytes from %s\n", (int)evt.packet->dataLength, ip);

			} else {
				printf("Unknown event from %s\n", ip);
			}
		} else {
			printf("... (%d peers)\n", (int)g_peers.size());
		}
	}

	enet_host_destroy(host);
	enet_deinitialize();
	return 0;
}

