#include <cstdio>
#include <cstdint>
#include <cstring>

#include <vector>
#include <algorithm>

#include <enet/enet.h>

#include <stdio.h>

const char* ServerHostName = "example.com";

enum PeerCommand : uint8_t
{
	pc_None,
	pc_PeerList,
	pc_NewPeer,
	pc_Ping,
	pc_Pong,
};

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

class QuickReadBuffer
{
private:
	const char* m_buffer;
	size_t m_size;
	size_t m_cursor;

public:
	QuickReadBuffer(const char* buffer, size_t size)
	{
		m_buffer = buffer;
		m_size = size;
		m_cursor = 0;
	}

	template<typename T>
	T Read()
	{
		T* ret = (T*)(m_buffer + m_cursor);
		m_cursor += sizeof(T);
		return *ret;
	}
};

static std::vector<ENetPeer*> g_remote_peers;

int main()
{
	QuickBuffer wb;

	enet_initialize();

	ENetAddress hostAddress;
	hostAddress.host = ENET_HOST_ANY;
	hostAddress.port = ENET_PORT_ANY;
	ENetHost* host = enet_host_create(&hostAddress, 100, 2, 0, 0);

	ENetAddress serverAddress;
	enet_address_set_host(&serverAddress, ServerHostName);
	serverAddress.port = 7788;
	ENetPeer* serverPeer = enet_host_connect(host, &serverAddress, 2, 0);

	while (true) {
		ENetEvent evt;
		if (enet_host_service(host, &evt, 1000) > 0) {
			char ip[40];
			enet_address_get_host_ip(&evt.peer->address, ip, 40);

			if (evt.type == ENET_EVENT_TYPE_CONNECT) {
				printf("Connected to %s:%d\n", ip, (int)evt.peer->address.port);

			} else if (evt.type == ENET_EVENT_TYPE_DISCONNECT) {
				printf("Disconnected from %s:%d\n", ip, (int)evt.peer->address.port);

				auto it = std::find(g_remote_peers.begin(), g_remote_peers.end(), evt.peer);
				if (it != g_remote_peers.end()) {
					g_remote_peers.erase(it);
				}

			} else if (evt.type == ENET_EVENT_TYPE_RECEIVE) {
				QuickReadBuffer b((const char*)evt.packet->data, evt.packet->dataLength);
				PeerCommand pc = b.Read<PeerCommand>();
				if (pc == pc_PeerList) {
					uint32_t numPeers = b.Read<uint32_t>();
					printf("%d peers from %s\n", (int)numPeers, ip);
					for (uint32_t i = 0; i < numPeers; i++) {
						ENetAddress addr;
						addr.host = b.Read<uint32_t>();
						addr.port = b.Read<uint16_t>();

						char peerListIp[40];
						enet_address_get_host_ip(&addr, peerListIp, 40);

						printf("- %s:%d\n", peerListIp, (int)addr.port);

						ENetPeer* peerRemote = enet_host_connect(host, &addr, 2, 0);
						g_remote_peers.emplace_back(peerRemote);
					}

				} else if (pc == pc_NewPeer) {
					ENetAddress addr;
					addr.host = b.Read<uint32_t>();
					addr.port = b.Read<uint16_t>();

					char newPeerIp[40];
					enet_address_get_host_ip(&addr, newPeerIp, 40);

					printf("New peer connected from %s: %s:%d\n", ip, newPeerIp, (int)addr.port);

					ENetPeer* peerRemote = enet_host_connect(host, &addr, 2, 0);
					g_remote_peers.emplace_back(peerRemote);

				} else if (pc == pc_Ping) {
					printf("Received ping from %s:%d\n", ip, (int)evt.peer->address.port);

					wb.Reset();
					wb.Write(pc_Pong);
					ENetPacket* packetPong = enet_packet_create(wb.GetBuffer(), wb.GetSize(), ENET_PACKET_FLAG_RELIABLE);
					enet_peer_send(evt.peer, 0, packetPong);

				} else if (pc == pc_Pong) {
					printf("Received pong from %s:%d\n", ip, (int)evt.peer->address.port);

				} else {
					printf("Unknown peer command %d from %s\n", (int)pc, ip);
				}

				enet_packet_destroy(evt.packet);

			} else {
				printf("Unknown event from %s\n", ip);
			}
		} else {
			printf("... (%d remote peers)\n", (int)g_remote_peers.size());
			for (auto peer : g_remote_peers) {
				wb.Reset();
				wb.Write(pc_Ping);
				ENetPacket* packetPing = enet_packet_create(wb.GetBuffer(), wb.GetSize(), ENET_PACKET_FLAG_RELIABLE);
				enet_peer_send(peer, 0, packetPing);
			}
		}
	}

	enet_host_destroy(host);
	enet_deinitialize();
	return 0;
}
