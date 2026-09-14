#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <poll.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <vector>
#include <algorithm>
#include "Protocol.h"

namespace
{
struct ClientState
{
    sockaddr_in address{};
    uint32_t lastTick = 0;
    uint64_t lastSeenMs = 0;
};

bool sameAddress(const sockaddr_in& left, const sockaddr_in& right)
{
    return left.sin_family == right.sin_family &&
           left.sin_port == right.sin_port &&
           left.sin_addr.s_addr == right.sin_addr.s_addr;
}

uint64_t nowMilliseconds()
{
    timeval time{};
    gettimeofday(&time, nullptr);
    return static_cast<uint64_t>(time.tv_sec) * 1000ULL +
           static_cast<uint64_t>(time.tv_usec) / 1000ULL;
}
}

int main()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    int reuseAddress = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                   &reuseAddress, sizeof(reuseAddress)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        close(sock);
        return 1;
    }

    // --- Set Non-blocking ---
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1 || fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        return 1;
    }

    // --- Set port recv ---
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);
    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        if (errno == EADDRINUSE) {
            std::fprintf(stderr,
                "UDP port 9000 is already in use. Stop the existing server first.\n");
        } else {
            perror("bind");
        }
        close(sock);
        return 1;
    }

    std::vector<ClientState> clients;
    pollfd socketPoll{};
    socketPoll.fd = sock;
    socketPoll.events = POLLIN;

    while (true)
    {
        const int ready = poll(&socketPoll, 1, 100);
        if (ready < 0)
        {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        if (ready > 0 && (socketPoll.revents & (POLLERR | POLLNVAL)))
        {
            std::fprintf(stderr, "UDP socket poll error\n");
            break;
        }

        if (ready > 0 && (socketPoll.revents & POLLIN))
        {
            while (true)
            {
                PlayerInputPacket packet{};
                sockaddr_in client{};
                socklen_t clientLen = sizeof(client);
                const ssize_t received = recvfrom(
                    sock,
                    &packet,
                    sizeof(packet),
                    0,
                    reinterpret_cast<sockaddr*>(&client),
                    &clientLen
                );

                if (received < 0)
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    if (errno == EINTR) {
                        continue;
                    }
                    perror("recvfrom");
                    break;
                }

                if (received != static_cast<ssize_t>(sizeof(packet)) ||
                    packet.header.type != PacketType::PLAYER_INPUT)
                {
                    continue;
                }

                const uint32_t tick = ntohl(packet.header.tick);
                const uint64_t seenAt = nowMilliseconds();
                auto clientIt = std::find_if(
                    clients.begin(),
                    clients.end(),
                    [&client](const ClientState& state) {
                        return sameAddress(state.address, client);
                    }
                );

                if (clientIt == clients.end())
                {
                    clients.push_back(ClientState{client, tick, seenAt});
                    clientIt = std::prev(clients.end());
                    std::printf("client connected: %s:%u (clients=%zu)\n",
                        inet_ntoa(client.sin_addr), ntohs(client.sin_port), clients.size());
                }
                else
                {
                    clientIt->lastTick = tick;
                    clientIt->lastSeenMs = seenAt;
                }

                std::printf(
                    "client %s:%u tick=%u up=%d down=%d left=%d right=%d run=%d\n",
                    inet_ntoa(client.sin_addr),
                    ntohs(client.sin_port),
                    tick,
                    packet.up,
                    packet.down,
                    packet.left,
                    packet.right,
                    packet.run
                );

                if (packet.attack) {
                    std::printf("client %s:%u attack\n",
                        inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                }

                EchoReplyPagket reply{};
                reply.header.type = PacketType::ECHO_REPLY;
                reply.header.tick = packet.header.tick;
                const ssize_t sent = sendto(
                    sock,
                    &reply,
                    sizeof(reply),
                    0,
                    reinterpret_cast<sockaddr*>(&client),
                    clientLen
                );
                if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("sendto");
                }
            }
        }

        const uint64_t currentTime = nowMilliseconds();
        clients.erase(
            std::remove_if(
                clients.begin(),
                clients.end(),
                [currentTime](const ClientState& client) {
                    return currentTime > client.lastSeenMs &&
                           currentTime - client.lastSeenMs > 10000;
                }
            ),
            clients.end()
        );
    }

    close(sock);
    return 0;
}