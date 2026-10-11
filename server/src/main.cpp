#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <poll.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
#include "Protocol.h"
#include "TcpServer.h"
#include <glm/glm.hpp>

namespace
{

constexpr float WALK_SPEED = 100.0f;
constexpr float RUN_SPEED  = 260.0f;

enum class Direction
{
    Up,
    Down,
    Left,
    Right
};

struct ClientState
{
    sockaddr_in address{};
    uint64_t lastSeenMs = 0;
};

struct PlayerState
{
    sockaddr_in address{};
    uint32_t playerID = 0;
    uint32_t lastProcessedSeq = 0;
    bool     hasSeq = false;
    uint64_t sessionToken = 0;
    uint64_t lastSeenMs = 0;

    glm::vec2   position{640.0f, 360.0f};
    glm::vec2   velocity{0.0f, 0.0f};

    uint8_t facing = 1; // Down
    bool attacking = false;
    bool running = false;
};

bool sameAddress(const sockaddr_in& left, const sockaddr_in& right)
{
    return left.sin_family == right.sin_family &&
           left.sin_port == right.sin_port &&
           left.sin_addr.s_addr == right.sin_addr.s_addr;
}

uint64_t nowMilliseconds()
{
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
    );
}
}

void applyinput(PlayerState &player, PlayerInputPacket& input)
{
    glm::vec2 moveDir{0.0f, 0.0f};
    if (input.up)    moveDir.y -= 1.0f;
    if (input.down)  moveDir.y += 1.0f;
    if (input.left)  moveDir.x -= 1.0f;
    if (input.right) moveDir.x += 1.0f;
    if (length(moveDir) > 0.0f)
    {
        moveDir = glm::normalize(moveDir);
        const float speed = input.run ? RUN_SPEED : WALK_SPEED;
        player.position += moveDir * speed * FIXED_DT;
        player.velocity = moveDir * speed;
        if (std::abs(moveDir.x) > std::abs(moveDir.y)) {
            player.facing = moveDir.x > 0.0f ? (uint8_t)Direction::Right : (uint8_t)Direction::Left;
        } else {
            player.facing = moveDir.y > 0.0f ? (uint8_t)Direction::Down : (uint8_t)Direction::Up;
        }
        player.running = input.run;
    }
    else
    {
        player.velocity = glm::vec2{0.0f};
    }
}

// --- Gửi ---
void sendPlayerState(int sock, const sockaddr_in& client, socklen_t clientLen, const PlayerState& player)
{
    PlayerStatePacket reply{};
    reply.header.type = PacketType::PLAYER_STATE;
    reply.header.seq  = htonl(player.lastProcessedSeq);
    reply.x    = player.position.x;
    reply.y    = player.position.y;
    reply.velX = player.velocity.x;
    reply.velY = player.velocity.y;
    reply.facing = player.facing;
    reply.flags  = (player.running ? 1 : 0) | (player.attacking ? 2 : 0);

    const ssize_t sent = sendto(
        sock,
        &reply,
        sizeof(reply),
        0,
        reinterpret_cast<const sockaddr*>(&client),
        clientLen
    );
    if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror("sendto");
    }
}

void handlePlayerInput(int sock, const sockaddr_in& client, socklen_t clientLen,
                        const char* buffer, ssize_t received,
                        std::vector<ClientState>& clients, PlayerState& player,
                        TcpServer& tcp, uint64_t nowMs)
{
    if (received != static_cast<ssize_t>(sizeof(PlayerInputPacket))) {
        return; // Kích thước không khớp type
    }

    PlayerInputPacket packet{};
    memcpy(&packet, buffer, sizeof(packet));

    if (!tcp.authenticateSession(packet.sessionToken, nowMs)) return;

    const uint64_t seenAt = nowMs;
    auto clientIt = std::find_if(
        clients.begin(),
        clients.end(),
        [&client](const ClientState& state) {
            return sameAddress(state.address, client);
        }
    );

    if (clientIt == clients.end())
    {
        clients.push_back(ClientState{client, seenAt});
        std::printf("client connected: %s:%u (clients=%zu)\n",
            inet_ntoa(client.sin_addr), ntohs(client.sin_port), clients.size());
    }
    else
    {
        clientIt->lastSeenMs = seenAt;
    }

    const uint32_t seq = ntohl(packet.header.seq);
    // Session mới (đăng nhập lại): seq của client bắt đầu lại từ đầu
    if (player.sessionToken != packet.sessionToken)
    {
        player.sessionToken = packet.sessionToken;
        player.hasSeq = false;
        player.lastProcessedSeq = 0;
    }

    if (!player.hasSeq || seq > player.lastProcessedSeq)
    {
        player.hasSeq = true;
        player.address = client;
        applyinput(player, packet);
        player.lastProcessedSeq = seq;
    }

    sendPlayerState(sock, client, clientLen, player);
}

// --- Nhận ---
void receivePackets(int sock, std::vector<ClientState>& clients, PlayerState& player,
                    TcpServer& tcp, uint64_t nowMs)
{
    while (true)
    {
        char buffer[1500];
        sockaddr_in client{};
        socklen_t clientLen = sizeof(client);
        const ssize_t received = recvfrom(
            sock,
            buffer,
            sizeof(buffer),
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

        if (received < static_cast<ssize_t>(sizeof(PacketHeader))) {
            continue; // Gói tin quá ngắn, không đủ header
        }

        PacketHeader header{};
        memcpy(&header, buffer, sizeof(header));

        switch (header.type)
        {
            case PacketType::PLAYER_INPUT:
                handlePlayerInput(sock, client, clientLen, buffer, received, clients, player,
                                  tcp, nowMs);
                break;
            default:
                break; // Loại gói tin chưa hỗ trợ hoặc không xác định
        }
    }
}

int main(int argc, char** argv)
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

    TcpServer tcp;
    if (!tcp.start(9000, argc > 1 ? argv[1] : "users.db", 9000)) {
        close(sock);
        return 1;
    }

    std::vector<ClientState> clients;
    PlayerState player{};
    std::vector<pollfd> fds;
    while (true)
    {
        fds.clear();
        fds.push_back(pollfd{sock, POLLIN, 0});   // fds[0] = UDP
        tcp.appendPollFds(fds);                   // fds[1..] = TCP listen + conns
        const int ready = poll(fds.data(), fds.size(), 100);
        if (ready < 0)
        {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        if (ready > 0 && (fds[0].revents & (POLLERR | POLLNVAL)))
        {
            std::fprintf(stderr, "UDP socket poll error\n");
            break;
        }

        const uint64_t currentTime = nowMilliseconds();

        if (ready > 0 && (fds[0].revents & POLLIN))
        {
            receivePackets(sock, clients, player, tcp, currentTime);
        }

        if (ready > 0) tcp.handleEvents(fds, 1, currentTime);
        tcp.sweep(currentTime);

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
