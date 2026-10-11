#include "Network.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstring>

namespace
{
constexpr size_t MAX_QUEUED_CHAT_BYTES = 64 * 1024;
constexpr size_t MAX_RECEIVED_CHAT_MESSAGES = 100;
}

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
    if (flag < 0 || fcntl(sock_, F_SETFL, flag | O_NONBLOCK) < 0) {
        perror("fcntl(udp)");
        close(sock_);
        sock_ = -1;
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, strIp.c_str(), &serverAddr.sin_addr) != 1 || uPort == 0) {
        std::fprintf(stderr, "Invalid UDP server address or fallback port.\n");
        close(sock_);
        sock_ = -1;
        return false;
    }
    serverIp_ = strIp;
    fallbackUdpPort_ = uPort;
    running_ = true;
    thread_ = std::thread(&Network::networkLoop, this);

    return true;
}

void Network::startTcpConnect(const std::string& strIp, uint16_t uPort, uint64_t nowTime)
{
    tcpSock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSock_ < 0) { failLogin(); return; }

    int flag = fcntl(tcpSock_, F_GETFL, 0);
    if (flag < 0 || fcntl(tcpSock_, F_SETFL, flag | O_NONBLOCK) < 0) {
        perror("fcntl(udp)");
        close(tcpSock_);
        tcpSock_ = -1;
        failLogin();
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(uPort);
    inet_pton(AF_INET, strIp.c_str(), &addr.sin_addr);

    loginStartMs_ = nowTime;
    const int r = connect(tcpSock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (r == 0) {                       // Kết nối thành công
        buildRequest();
        connState_ = ConnState::TcpSending;
    } else if (errno == EINPROGRESS) {  // Đang tiến hành xử lí login
        connState_ = ConnState::TcpConnecting;
    } else {
        failLogin();
    }
}

void Network::beginLogin(const std::string& name, const std::string& pass)
{
    std::lock_guard<std::mutex> lock(loginMutex_);
    pendingName_ = name;
    pendingPass_ = pass;
    lastStatus_ = 0;
    connState_ = ConnState::Disconnected;   // xoá trạng thái Failed cũ trước khi thread mạng xử lí
    loginRequested_ = true;
}

void Network::pumpLogin(uint64_t nowTime, short int revents)
{
    const ConnState state = connState_.load();
    if ((state == ConnState::TcpConnecting ||
         state == ConnState::TcpSending ||
         state == ConnState::TcpReceiving) &&
        nowTime - loginStartMs_ > LOGIN_TIMEOUT_MS) {
        failLogin();
        return;
    }

    switch (state)
    {
        case ConnState::TcpConnecting:
        {
            if (revents & POLLNVAL) { failLogin(); return; }
            if (revents & (POLLOUT | POLLERR | POLLHUP))
            {
                int err = 0; socklen_t len = sizeof(err);
                if (getsockopt(tcpSock_, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
                    failLogin();                         // err = ECONNREFUSED / EHOSTUNREACH...
                    return;
                }
                buildRequest();
                connState_ = ConnState::TcpSending;
            }
            break;
        }
        case ConnState::TcpSending: 
        {
            if (revents & (POLLERR | POLLHUP | POLLNVAL)) { failLogin(); return; }
            if (!(revents & POLLOUT)) return;
            const ssize_t n = send(tcpSock_, txBuf_.data() + txSent_,
                                txBuf_.size() - txSent_, MSG_NOSIGNAL);
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) return;
                failLogin(); return;
            }
            txSent_ += n;       // send có thể gửi thiếu
            if (txSent_ == txBuf_.size()) {
                rxGot_ = 0;
                connState_ = ConnState::TcpReceiving;
            }
            break;
        }
        case ConnState::TcpReceiving:
        {
            if (revents & POLLNVAL) { failLogin(); return; }
            if (!(revents & POLLIN)) {
                if (revents & (POLLERR | POLLHUP)) failLogin();
                return;
            }
            const ssize_t n = recv(tcpSock_, rxBuf_ + rxGot_, sizeof(rxBuf_) - rxGot_, 0);
            if (n == 0) { failLogin(); return; }        // server đóng
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) return;
                failLogin(); return;
            }
            rxGot_ += n;
            if (rxGot_ == sizeof(rxBuf_)) finishLogin();
                break;
        }
        case ConnState::Connected:
        {
            if (revents & (POLLERR | POLLHUP | POLLNVAL)) { failLogin(); return; }
            if ((revents & POLLOUT) && !flushChat()) { failLogin(); return; }
            if ((revents & POLLIN) && !receiveChat()) { failLogin(); return; }
            break;
        }
        default: break;
    }

}

void Network::buildRequest()
{
    LoginRequestMsg req{};  // zero-fill
    std::string name, pass;
    {
        std::lock_guard<std::mutex> lock(loginMutex_);
        name = pendingName_;
        pass = pendingPass_;
    }

    memcpy(req.name,     name.data(), std::min(name.size(), sizeof(req.name)));
    memcpy(req.password, pass.data(), std::min(pass.size(), sizeof(req.password)));

    txBuf_.resize(sizeof(req));
    memcpy(txBuf_.data(), &req, sizeof(req));
    txSent_ = 0;                                 // reset con trỏ gửi
}

void Network::finishLogin()
{
    LoginResponseMsg resp{};
    memcpy(&resp, rxBuf_, sizeof(resp));

    lastStatus_ = resp.status;

    if (resp.status != static_cast<uint8_t>(LoginStatus::OK))
    {
        closeTcp();
        connState_ = ConnState::Failed;
        return;
    }

    playerID_     = ntohl(resp.playerID);
    sessionToken_ = resp.sessionToken;
    udpPort_      = ntohs(resp.udpPort);
    if (udpPort_ == 0) udpPort_ = fallbackUdpPort_;
    if (!connectUdp(udpPort_)) {
        failLogin();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        latestState_ = ServerState{};   // bỏ state cũ của phiên trước
    }
    {
        std::lock_guard<std::mutex> lock(inputMutex_);
        sentHistory_.clear();
    }
    nextSeq_ = 0;
    connState_ = ConnState::Connected;
}

bool Network::connectUdp(uint16_t port)
{
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (port == 0 || inet_pton(AF_INET, serverIp_.c_str(), &serverAddr.sin_addr) != 1) {
        std::fprintf(stderr, "Invalid UDP server endpoint.\n");
        return false;
    }

    if (connect(sock_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        perror("connect(udp)");
        return false;
    }
    return true;
}

void Network::closeTcp()
{
    if (tcpSock_ >= 0) {
        close(tcpSock_);
        tcpSock_ = -1;
    }
}

void Network::failLogin()
{
    connState_ = ConnState::Failed;
    closeTcp();
    txBuf_.clear();
    txSent_ = 0;
    rxGot_  = 0;
    tcpRxBuffer_.clear();
    {
        std::lock_guard<std::mutex> lock(chatMutex_);
        chatTxQueue_.clear();
        chatTxOffset_ = 0;
        chatTxQueuedBytes_ = 0;
    }

    lastStatus_ = LOGIN_NET_ERROR;
}

void Network::stop()
{
    running_ = false;
    if (thread_.joinable()) thread_.join();
    closeTcp();
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

    // --- Tích luỹ các phím đã được giữ kể từ lần gửi trước ---
    // Tránh mất input khi người chơi bấm-nhả nhanh hơn chu kỳ gửi TICK_INTERVAL_MS.
    pendingSendInput_.up |= up;
    pendingSendInput_.down |= down;
    pendingSendInput_.left |= left;
    pendingSendInput_.right |= right;
    pendingSendInput_.attack |= attack;
    pendingSendInput_.run |= run;
}

bool Network::sendChatMessage(const std::string& text)
{
    if (text.empty() || text.size() >= MAX_CHAT_TEXT_LEN ||
        connState_.load() != ConnState::Connected) {
        return false;
    }

    ChatMessagePayload payload{};
    std::memcpy(payload.text, text.data(), text.size());

    TcpFrameHeader header{};
    header.length = htons(static_cast<uint16_t>(sizeof(payload)));
    header.type = static_cast<uint8_t>(TcpMsgType::CHAT_MESSAGE);

    std::vector<char> frame(sizeof(header) + sizeof(payload));
    std::memcpy(frame.data(), &header, sizeof(header));
    std::memcpy(frame.data() + sizeof(header), &payload, sizeof(payload));

    std::lock_guard<std::mutex> lock(chatMutex_);
    if (connState_.load() != ConnState::Connected) return false;
    if (chatTxQueuedBytes_ + frame.size() > MAX_QUEUED_CHAT_BYTES) {
        return false;
    }
    chatTxQueuedBytes_ += frame.size();
    chatTxQueue_.push_back(std::move(frame));
    return true;
}

std::vector<ChatMessage> Network::takeChatMessages()
{
    std::lock_guard<std::mutex> lock(chatMutex_);
    std::vector<ChatMessage> messages;
    messages.reserve(chatRxQueue_.size());
    while (!chatRxQueue_.empty()) {
        messages.push_back(std::move(chatRxQueue_.front()));
        chatRxQueue_.pop_front();
    }
    return messages;
}

bool Network::hasPendingChat()
{
    std::lock_guard<std::mutex> lock(chatMutex_);
    return !chatTxQueue_.empty();
}

bool Network::flushChat()
{
    std::lock_guard<std::mutex> lock(chatMutex_);
    while (!chatTxQueue_.empty())
    {
        std::vector<char>& frame = chatTxQueue_.front();
        const ssize_t sent = send(tcpSock_, frame.data() + chatTxOffset_,
                                  frame.size() - chatTxOffset_, MSG_NOSIGNAL);
        if (sent > 0) {
            chatTxOffset_ += static_cast<size_t>(sent);
            if (chatTxOffset_ == frame.size()) {
                chatTxQueuedBytes_ -= frame.size();
                chatTxQueue_.pop_front();
                chatTxOffset_ = 0;
            }
            continue;
        }
        if (sent < 0 && errno == EINTR) continue;
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
        return false;
    }
    return true;
}

bool Network::receiveChat()
{
    char buffer[1024];
    while (true)
    {
        const ssize_t received = recv(tcpSock_, buffer, sizeof(buffer), 0);
        if (received > 0) {
            tcpRxBuffer_.insert(tcpRxBuffer_.end(), buffer, buffer + received);
            if (!parseChatFrames()) return false;
        } else if (received == 0) {
            return false;
        } else if (errno == EINTR) {
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        } else {
            return false;
        }
    }
}

bool Network::parseChatFrames()
{
    constexpr size_t headerSize = sizeof(TcpFrameHeader);
    constexpr size_t payloadSize = sizeof(ChatMessagePayload);
    while (tcpRxBuffer_.size() >= headerSize)
    {
        TcpFrameHeader header{};
        std::memcpy(&header, tcpRxBuffer_.data(), headerSize);
        const uint16_t length = ntohs(header.length);
        if (header.type != static_cast<uint8_t>(TcpMsgType::CHAT_MESSAGE) ||
            length != payloadSize) {
            return false;
        }

        const size_t frameSize = headerSize + length;
        if (tcpRxBuffer_.size() < frameSize) return true;

        ChatMessagePayload payload{};
        std::memcpy(&payload, tcpRxBuffer_.data() + headerSize, sizeof(payload));
        const size_t textLength = strnlen(payload.text, sizeof(payload.text));
        if (textLength == 0 || textLength == sizeof(payload.text)) return false;

        ChatMessage message;
        message.playerID = ntohl(payload.playerID);
        message.sender.assign(payload.sender, strnlen(payload.sender, sizeof(payload.sender)));
        message.text.assign(payload.text, textLength);
        {
            std::lock_guard<std::mutex> lock(chatMutex_);
            if (chatRxQueue_.size() == MAX_RECEIVED_CHAT_MESSAGES)
                chatRxQueue_.pop_front();
            chatRxQueue_.push_back(std::move(message));
        }
        tcpRxBuffer_.erase(tcpRxBuffer_.begin(),
                           tcpRxBuffer_.begin() + static_cast<std::ptrdiff_t>(frameSize));
    }
    return true;
}

void Network::networkLoop()
{
    uint64_t lastSendTime = 0;
    while (running_)
    {
        uint64_t nowTime = SDL_GetTicks();
        bool didWork = false;

        if (loginRequested_.exchange(false))
            startTcpConnect(serverIp_, fallbackUdpPort_, nowTime);

        pollfd p{tcpSock_, 0, 0};
        bool needPoll = false;

        ConnState st = connState_.load();
        switch (st) {
            case ConnState::TcpConnecting:
                p.events = POLLOUT;  needPoll = true; break;
            case ConnState::TcpSending:
                p.events = POLLOUT;  needPoll = true; break;
            case ConnState::TcpReceiving:
                p.events = POLLIN;   needPoll = true; break;
            case ConnState::Connected:
                p.events = POLLIN;
                if (hasPendingChat()) p.events |= POLLOUT;
                needPoll = true;
                break;
            default: break;
        }

        if (needPoll) {
            const int ready = poll(&p, 1, st == ConnState::Connected ? 0 : 10);
            if (ready > 0) {
                pumpLogin(nowTime, p.revents);
                didWork = true;
            } else if (ready == 0 || errno == EINTR) {
                checkTimeout(nowTime);
            } else {
                perror("poll(tcp)");
                failLogin();
            }
        }

        if (st != ConnState::Connected) {      // chưa login thì chưa bắn UDP
            if (!didWork)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            continue;
        }

        InputSnapshot snap;
        {
            std::lock_guard<std::mutex> lock(inputMutex_);
            snap = pendingSendInput_;
        }

        bool shouldSend = snap.attack || (nowTime - lastSendTime >= TICK_INTERVAL_MS);

        // --- Quyết định gửi ---
        if (shouldSend)
        {
            PlayerInputPacket pkt{};
            pkt.header.type = PacketType::PLAYER_INPUT;
            const uint32_t seq = nextSeq_++;
            pkt.header.seq = htonl(seq);
            pkt.sessionToken = sessionToken_;
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
                // --- Reset accumulator, giữ lại phím vẫn đang được nhấn ---
                std::lock_guard<std::mutex> lock(inputMutex_);
                pendingSendInput_ = currentInput_;
                sentHistory_.push_back({seq, snap.up, snap.down, snap.left, snap.right, snap.run});
                while (sentHistory_.size() > 128) sentHistory_.pop_front();
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

            lastReceivedTime_ = nowTime;
            handlePacket(buffer, n);
        }

        checkServerTimeout(nowTime);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Network::handlePacket(const char* buffer, ssize_t n)
{
    if (n < static_cast<ssize_t>(sizeof(PacketHeader))) {
        return; // Gói tin quá ngắn, không đủ header
    }

    PacketHeader header{};
    memcpy(&header, buffer, sizeof(header));

    switch (header.type)
    {
        case PacketType::PLAYER_STATE:
        {
            if (n != sizeof(PlayerStatePacket)) break; // Kích thước không khớp type
            PlayerStatePacket state{};
            memcpy(&state, buffer, sizeof(state));
            const uint32_t seq = ntohl(state.header.seq);

            std::lock_guard<std::mutex> lock(stateMutex_);
            if (latestState_.valid && seq <= latestState_.lastProcessedSeq) {
                break; // Gói cũ / đến sai thứ tự, bỏ toàn bộ
            }

            latestState_.lastProcessedSeq = seq;
            latestState_.x = state.x;
            latestState_.y = state.y;
            latestState_.velX = state.velX;
            latestState_.velY = state.velY;
            latestState_.facing = state.facing;
            latestState_.valid = true;
            break;
        }
        default:
            break;
    }
}

void Network::checkServerTimeout(uint64_t nowTime)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (latestState_.valid && nowTime - lastReceivedTime_ > SERVER_TIMEOUT_MS)
    {
        latestState_.valid = false;
    }
}

ServerState Network::getSeverState()
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    return latestState_;
}

std::vector<SentInput> Network::getUnackedInputs(uint32_t ackSeq)
{
    std::lock_guard<std::mutex> lock(inputMutex_);
    std::vector<SentInput> out;
    for (const SentInput& in : sentHistory_)
        if (in.seq > ackSeq) out.push_back(in);
    return out;
}

void Network::checkTimeout(uint64_t nowTime)
{
    ConnState st = connState_.load();

    // Login timeout
    if (st == ConnState::TcpConnecting ||
        st == ConnState::TcpSending   ||
        st == ConnState::TcpReceiving)
    {
        if (nowTime - loginStartMs_ > LOGIN_TIMEOUT_MS) {
            failLogin();
            return;
        }
    }
}