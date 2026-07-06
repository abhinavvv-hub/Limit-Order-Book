#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>
#include <atomic>
#include "system/Queue.h"

/*
Simple UDP multicast receiver using the Windows Socket API.
Designed to run in its own thread, passing parsed messages to the SPSC queue.
*/
class UdpReceiver {
public:
    UdpReceiver() : sock(INVALID_SOCKET) {}

    ~UdpReceiver() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
        }
    }

    /* Initialise and join a multicast group. */
    bool init(const char* multicast_ip, uint16_t port) {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) {
            printf("socket() failed: %d\n", WSAGetLastError());
            return false;
        }

        /* Allow multiple processes to bind to the same port */
        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

        /* Bind to any local interface on the given port */
        sockaddr_in local{};
        local.sin_family      = AF_INET;
        local.sin_port        = htons(port);
        local.sin_addr.s_addr = INADDR_ANY;
        if (bind(sock, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR) {
            printf("bind() failed: %d\n", WSAGetLastError());
            return false;
        }

        /* Join the multicast group */
        ip_mreq mreq{};
        inet_pton(AF_INET, multicast_ip, &mreq.imr_multiaddr);
        mreq.imr_interface.s_addr = INADDR_ANY;
        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR) {
            printf("IP_ADD_MEMBERSHIP failed: %d\n", WSAGetLastError());
            return false;
        }

        return true;
    }

    /* Receive loop. Runs until 'running' becomes false. */
    template <size_t QCapacity>
    void run(SPSCQueue<OrderRequest, QCapacity>& queue, std::atomic<bool>& running) {
        OrderRequest msg;
        sockaddr_in from{};
        int fromlen = sizeof(from);

        while (running) {
            int bytes = recvfrom(sock, reinterpret_cast<char*>(&msg), sizeof(msg), 0, reinterpret_cast<sockaddr*>(&from), &fromlen);
            if (bytes == SOCKET_ERROR) {
                printf("recvfrom error: %d\n", WSAGetLastError());
                continue;
            }
            if (static_cast<size_t>(bytes) != sizeof(msg)) {
                continue; 
            }
            /*
            Try to push into the lock‑free queue.
            If the queue is full, we silently drop the message (production system
            would increment a drop counter for monitoring).
            */            
            if (!queue.push(msg)) [[unlikely]] {
                /* Could log a drop, but keeping the hot path fast. */
            }
        }
    }

private:
    SOCKET sock;
};
