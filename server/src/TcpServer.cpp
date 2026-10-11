#include "TcpServer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <algorithm>
#include <sqlite3.h>
#include "Database.h"

namespace
{
constexpr float SPAWN_X = 640.0f;
constexpr float SPAWN_Y = 360.0f;
constexpr size_t MAX_QUEUED_TCP_BYTES = 64 * 1024;

bool setNonBlocking(int fd)
{
    const int fl = fcntl(fd, F_GETFL, 0);
    return fl != -1 && fcntl(fd, F_SETFL, fl | O_NONBLOCK) != -1;
}

// Token chưa được kiểm tra ở đâu cả, mới chỉ phát cho client.
uint64_t randomToken()
{
    static std::mt19937_64 rng{std::random_device{}()};
    return rng();
}
}

TcpServer::~TcpServer()
{
    for (Conn& c : conns_) close(c.fd);
    if (listenFd_ >= 0) close(listenFd_);
}

// --- Khởi động ---
bool TcpServer::loadUsers(const std::string& path)
{
    if (!db.open(path)) {
        return false;
    }

    // Tạo bảng mới nếu không có sẵn (chạy lần đầu)
    const char* createSql =
        "CREATE TABLE IF NOT EXISTS users ("
        "  name     TEXT PRIMARY KEY,"
        "  password TEXT NOT NULL"
        ");";
    if (!db.exec(createSql)) {
        return false;
    }
    return true;
}

bool TcpServer::start(uint16_t port, const std::string& usersPath, uint16_t udpPort)
{
    udpPort_ = udpPort;
    if (!loadUsers(usersPath)) return false;

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) { perror("socket(tcp)"); return false; }

    int on = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    if (!setNonBlocking(listenFd_)) { perror("fcntl(tcp)"); return false; }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);
    if (bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        if (errno == EADDRINUSE)
            std::fprintf(stderr, "TCP port %u is already in use.\n", port);
        else
            perror("bind(tcp)");
        return false;
    }
    if (listen(listenFd_, 16) < 0) { perror("listen"); return false; }
    return true;
}

void TcpServer::acceptAll(uint64_t nowMs)
{
    while (true)
    {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        const int fd = accept(listenFd_, reinterpret_cast<sockaddr*>(&peer), &len);

        if (fd < 0)
        {
            if (errno ==  EINTR )                           continue;
            if (errno != EAGAIN  && errno != EWOULDBLOCK)   perror("accept");
            return;
        }

        if (conns_.size() >= MAX_CONNS || !setNonBlocking(fd))
        {
            close(fd);
            continue;
        }

        Conn c;
        c.fd = fd;
        c.startMs = nowMs;
        conns_.push_back(std::move(c));
        printf("tcp connect: %s : %u", inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
    }
}

// --- Quản lý kết nối ---
TcpServer::Conn* TcpServer::findConn(int fd)
{
    auto it = std::find_if(conns_.begin(), conns_.end(), [fd](const Conn& c) {return c.fd == fd;});
    return it == conns_.end() ? nullptr : &*it;
}

void TcpServer::closeConn(int fd)
{
    const auto it = std::find_if(conns_.begin(), conns_.end(),
                                 [fd](const Conn& c) { return c.fd == fd; });
    if (it == conns_.end()) return;
    close(fd);
    conns_.erase(it);
}

// --- Poll ---
void TcpServer::appendPollFds(std::vector<pollfd>& fds) const
{
    fds.push_back(pollfd{listenFd_ , POLLIN, 0});
    for (const Conn& c : conns_)
    {
        short events = 0;
        if (!c.replied) {
            events = POLLIN;
        } else if (c.authenticated) {
            events = POLLIN;
            if (c.txSent < c.tx.size()) events |= POLLOUT;
        } else if (c.txSent < c.tx.size()) {
            events = POLLOUT;
        }
        fds.push_back(pollfd{c.fd, events, 0});
    }
}

void TcpServer::handleEvents(const std::vector<pollfd>& fds, size_t first, uint64_t nowMs)
{
    for (size_t i = first; i < fds.size(); ++i)
    {
        const pollfd& p = fds[i];
        if (p.revents == 0)         continue;

        if (p.fd == listenFd_) {
            if (p.revents & POLLIN) acceptAll(nowMs);
            continue;
        }

        // Tìm theo fd
        Conn* c = findConn(p.fd);
        if (c && !serve(*c, p.revents, nowMs)) closeConn(p.fd);
    }

    std::sort(deferredClose_.begin(), deferredClose_.end());
    deferredClose_.erase(std::unique(deferredClose_.begin(), deferredClose_.end()),
                         deferredClose_.end());
    for (int fd : deferredClose_) closeConn(fd);
    deferredClose_.clear();
}

void TcpServer::broadcastChat(const ChatMessagePayload& message)
{
    for (Conn& conn : conns_)
    {
        if (conn.authenticated && !queueFrame(conn, message)) {
            std::fprintf(stderr, "Closing slow TCP chat client (fd=%d): send queue limit exceeded.\n",
                         conn.fd);
            deferredClose_.push_back(conn.fd);
        }
    }
}

bool TcpServer::queueFrame(Conn& conn, const ChatMessagePayload& message)
{
    TcpFrameHeader header{};
    header.length = htons(static_cast<uint16_t>(sizeof(message)));
    header.type = static_cast<uint8_t>(TcpMsgType::CHAT_MESSAGE);

    const size_t frameSize = sizeof(header) + sizeof(message);
    if (conn.tx.size() - conn.txSent + frameSize > MAX_QUEUED_TCP_BYTES) {
        return false;
    }

    if (conn.txSent > 0) {
        conn.tx.erase(conn.tx.begin(), conn.tx.begin() + static_cast<std::ptrdiff_t>(conn.txSent));
        conn.txSent = 0;
    }
    const char* headerBytes = reinterpret_cast<const char*>(&header);
    const char* messageBytes = reinterpret_cast<const char*>(&message);
    conn.tx.insert(conn.tx.end(), headerBytes, headerBytes + sizeof(header));
    conn.tx.insert(conn.tx.end(), messageBytes, messageBytes + sizeof(message));
    return true;
}

void TcpServer::sweep(uint64_t nowMs)
{
    std::vector<int> expired;
    for (const Conn& c : conns_)
    {
        if (!c.authenticated && nowMs - c.startMs >= LOGIN_TIMEOUT_MS)
            expired.push_back(c.fd);
    }

    for (int fd : expired)
        closeConn(fd);

    std::vector<uint64_t> expiredSessions;
    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        if (nowMs - it->second.lastSeenMs >= SESSION_TIMEOUT_MS) {
            expiredSessions.push_back(it->first);
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = conns_.begin(); it != conns_.end();)
    {
        const bool sessionExpired = std::find(
            expiredSessions.begin(), expiredSessions.end(), it->sessionToken) != expiredSessions.end();
        if (it->authenticated && sessionExpired) {
            close(it->fd);
            it = conns_.erase(it);
        } else {
            ++it;
        }
    }
}

bool TcpServer::authenticateSession(uint64_t token, uint64_t nowMs)
{
    const auto it = sessions_.find(token);
    if (it == sessions_.end()) return false;
    if (nowMs - it->second.lastSeenMs >= SESSION_TIMEOUT_MS)
    {
        sessions_.erase(it);
        return false;
    }
    it->second.lastSeenMs = nowMs;
    return true;
}

// --- Xử lý một kết nối ---
bool TcpServer::serve(Conn& c, short revents, uint64_t nowMs)
{
    if (revents & (POLLERR | POLLNVAL | POLLHUP)) return false;

    if (!c.replied)
    {
        if (!(revents & POLLIN)) return true;
        if (!readRequest(c))        return false;
        if (c.rxGot < sizeof(c.rx)) return true;    // Chờ phần request còn lại.
        LoginRequestMsg req;
        memcpy(&req, &c.rx, sizeof(req));
        LoginResponseMsg resp{};
        const bool accepted = makeResponse(req, nowMs, resp);
        c.tx.resize(sizeof(resp));
        std::memcpy(c.tx.data(), &resp, sizeof(resp));
        c.replied = true;
        c.authenticated = accepted;
        c.closeAfterFlush = !accepted;
        if (accepted) {
            c.playerID = ntohl(resp.playerID);
            c.sessionToken = resp.sessionToken;
            c.playerName.assign(req.name, strnlen(req.name, sizeof(req.name)));
        }
        return true;
    }

    if (c.authenticated && (revents & POLLIN) && !readChatFrames(c, nowMs))
        return false;

    if ((revents & POLLOUT) && !flush(c))
        return false;

    return !c.closeAfterFlush || !c.tx.empty();
}

bool TcpServer::readChatFrames(Conn& c, uint64_t nowMs)
{
    char buffer[1024];
    while (true)
    {
        const ssize_t received = recv(c.fd, buffer, sizeof(buffer), 0);
        if (received > 0) {
            if (c.frameRx.size() + static_cast<size_t>(received) > MAX_QUEUED_TCP_BYTES)
                return false;
            c.frameRx.insert(c.frameRx.end(), buffer, buffer + received);
        } else if (received == 0) {
            return false;
        } else if (errno == EINTR) {
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        } else {
            return false;
        }
    }

    constexpr size_t headerSize = sizeof(TcpFrameHeader);
    constexpr size_t payloadSize = sizeof(ChatMessagePayload);
    if (c.frameRx.size() > MAX_QUEUED_TCP_BYTES) return false;

    while (c.frameRx.size() >= headerSize)
    {
        TcpFrameHeader header{};
        std::memcpy(&header, c.frameRx.data(), headerSize);
        const uint16_t length = ntohs(header.length);
        if (header.type != static_cast<uint8_t>(TcpMsgType::CHAT_MESSAGE) ||
            length != payloadSize) {
            return false;
        }

        const size_t frameSize = headerSize + length;
        if (c.frameRx.size() < frameSize) break;

        ChatMessagePayload incoming{};
        std::memcpy(&incoming, c.frameRx.data() + headerSize, sizeof(incoming));
        const size_t textLength = strnlen(incoming.text, sizeof(incoming.text));
        if (textLength == 0 || textLength == sizeof(incoming.text)) return false;
        if (!authenticateSession(c.sessionToken, nowMs)) return false;

        ChatMessagePayload broadcast{};
        broadcast.playerID = htonl(c.playerID);
        std::memcpy(broadcast.sender, c.playerName.data(),
                    std::min(c.playerName.size(), sizeof(broadcast.sender)));
        std::memcpy(broadcast.text, incoming.text, textLength);
        broadcastChat(broadcast);
        c.frameRx.erase(c.frameRx.begin(),
                        c.frameRx.begin() + static_cast<std::ptrdiff_t>(frameSize));
    }
    return true;
}

bool TcpServer::readRequest(Conn& c)
{
    while (c.rxGot < sizeof(c.rx))
    {
        const ssize_t  n = recv(c.fd, c.rx + c.rxGot, sizeof(c.rx) - c.rxGot, 0);
        if (n > 0)
        {
            c.rxGot += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR)                            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;    // Hiện chưa có dữ liệu, quay lại poll
        return false;   // n == 0, hoặc lỗi
    }
    return true;
}

bool TcpServer::flush(Conn& c)
{
    while (c.txSent < c.tx.size())
    {
        const ssize_t  n = send(c.fd, c.tx.data() + c.txSent, c.tx.size() - c.txSent,
                                MSG_NOSIGNAL);
        if (n > 0)
        {
            c.txSent += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR)                            continue;       // Thử lại
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
        return false;
    }
    c.tx.clear();
    c.txSent = 0;
    return true;
}

bool TcpServer::makeResponse(const LoginRequestMsg& req, uint64_t nowMs, LoginResponseMsg& resp)
{
    const std::string name(req.name, strnlen(req.name, sizeof(req.name)));
    const std::string password(req.password, strnlen(req.password, sizeof(req.password)));

    std::string storedPassword;
    if (!db.getUserPassword(name, storedPassword))
    {
        resp.status = static_cast<uint8_t>(LoginStatus::NAME_TAKEN);
        return false;
    }

    if (storedPassword != password)
    {
        resp.status = static_cast<uint8_t>(LoginStatus::WRONG_PASSWORD);
        return false;
    }

    const uint32_t playerID = nextPlayerID_++;

    resp.status = static_cast<uint8_t>(LoginStatus::OK);
    resp.playerID = htonl(playerID);
    resp.sessionToken = randomToken();
    sessions_[resp.sessionToken] = Session{nowMs};
    resp.udpPort = htons(udpPort_);
    resp.spawnX       = SPAWN_X;
    resp.spawnY       = SPAWN_Y;
    std::printf("login ok: '%s' -> playerID=%u\n", name.c_str(), playerID);
    return true;
}
