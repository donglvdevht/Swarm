#include "Network.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <SDL3/SDL.h>

Network::Network() = default;
Network::~Network(){
    stop();
}

bool Network::start(const std::string& strIp, uint16_t uPort){
    // --- Create UDP socket ---
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0) {
        perror("socket");
        return false;
    }

    // --- Set non-blocking ---
    int flag = fcntl(sock_, F_GETFL, 0);
    fcntl(sock_, F_SETFL, flag | O_NONBLOCK);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(uPort);
    inet_pton(AF_INET, strIp.c_str(), &serverAddr.sin_addr);

    connect(sock_, (sockaddr*)&serverAddr, sizeof(serverAddr));
    running_ = true;
    thread_ = std::thread(&Network::networkLoop, this);

    return true;
}

void Network::stop()
{
    running_ = false;
    if (thread_.joinable()) thread_.join();
    close(sock_);
    sock_ = -1;
}

void Network::updateInput(bool up, bool down, bool left, bool right, bool attack, bool run)
{
    std::lock_guard<std::mutex> lock(inputMutex_);
    currentInput_.up = up;
    currentInput_.down = down;
    currentInput_.left = left;
    currentInput_.right = right;
    currentInput_.attack = attack;
    currentInput_.run = run;
}

void Network::networkLoop()
{
    uint64_t lastSendTime = 0;
    while (running_)
    {
        InputSnapshot snap;
        {
            std::lock_guard<std::mutex> lock(inputMutex_);
            snap = currentInput_;
        }
        uint64_t nowTime = SDL_GetTicks();
        bool shouldSend = snap.attack || (nowTime - lastSendTime >= SEND_INTERVAL_MS);

        // --- Quyết định gửi ---
        if (shouldSend)
        {
            PlayerInputPacket pkt{};
            pkt.header.type = PacketType::PLAYER_INPUT;
            pkt.header.seq = htonl(nextSeq_++);;
            pkt.up = snap.up;
            pkt.down = snap.down;
            pkt.left = snap.left;
            pkt.right = snap.right;
            pkt.attack = snap.attack;
            pkt.run = snap.run;

            const ssize_t sent = send(sock_, &pkt, sizeof(pkt), 0);
            if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("send");
            }
            else if (sent == static_cast<ssize_t>(sizeof(pkt))) {
                lastSendTime = nowTime;
            }
        }

        // --- Nhận dữ liệu từ sever ---

        char buffer[1500];
        while (true)
        {
            const ssize_t n = recv(sock_, buffer, sizeof(buffer), 0);
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;  // No data
                }
                perror("recv");
                break;
            }

            if (n == sizeof(PlayerStatePacket))
            {
                PlayerStatePacket state{};
                memcpy(&state, buffer, sizeof(state));
                if (state.header.type == PacketType::PLAYER_STATE)
                {
                    std::lock_guard<std::mutex> lock(stateMutex_);
                    latestState_.lastProcessedSeq = ntohl(state.header.seq);
                    latestState_.x = state.x;
                    latestState_.y = state.y;
                    latestState_.velX = state.velX;
                    latestState_.velY = state.velY;
                    latestState_.facing = state.facing;
                    latestState_.valid = true;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

ServerState Network::getSeverState()
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    return latestState_;
}

uint32_t Network::getCurrentSeq()
{
    return nextSeq_;
}