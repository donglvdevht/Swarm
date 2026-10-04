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
#include <nlohmann/json.hpp>

namespace
{
constexpr float SPAWN_X = 640.0f;
constexpr float SPAWN_Y = 360.0f;

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
    std::ifstream file(path);

    if (!file)
    {
        printf("Can't open");
        return false;
    }

    try
    {
        const nlohmann::json doc = nlohmann::json::parse(file);
        for (const auto& u : doc.at("users"))
        {
            users_[u.at("name").get<std::string>()] = u.at("password").get<std::string>();
        }
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "bad %s: %s\n", path.c_str(), e.what());
        return false;
    }

    std::printf("loaded %zu users from %s\n", users_.size(), path.c_str());

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
    close(fd);
    conns_.erase(std::remove_if(conns_.begin(), conns_.end(),
                                [fd](const Conn& c) { return c.fd == fd; }),
                 conns_.end());
}

// --- Poll ---
void TcpServer::appendPollFds(std::vector<pollfd>& fds) const
{
    fds.push_back(pollfd{listenFd_ , POLLIN, 0});
    for (const Conn& c : conns_)
    {
        const short events = static_cast<short>(c.replied ? POLLOUT : POLLIN);
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
}

void TcpServer::sweep(uint64_t nowMs)
{
    std::vector<int> expired;
    for (const Conn& c : conns_)
    {
        if (nowMs - c.startMs >= LOGIN_TIMEOUT_MS)
            expired.push_back(c.fd);
    }

    for (int fd : expired)
        closeConn(fd);

    for (auto it = sessions_.begin(); it != sessions_.end();)
    {
        if (nowMs - it->second.lastSeenMs >= SESSION_TIMEOUT_MS)
            it = sessions_.erase(it);
        else
            ++it;
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
    if (revents & (POLLERR | POLLNVAL)) return false;

    if (!c.replied)
    {
        if (!readRequest(c))        return false;
        if (c.rxGot < sizeof(c.rx)) return true;    // Chờ phần request còn lại.
        LoginRequestMsg req;
        memcpy(&req, &c.rx, sizeof(req));
        LoginResponseMsg resp;
        makeResponse(req, nowMs, resp);
        c.tx.resize(sizeof(resp));
        std::memcpy(c.tx.data(), &resp, sizeof(resp));
        c.replied = true;
    }
    return !flush(c);
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
        const ssize_t  n = send(c.fd, c.tx.data() + c.txSent, c.tx.size() - c.txSent, 0);
        if (n > 0)
        {
            c.txSent += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR)                            continue;       // Thử lại
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return false;   // Socket chưa thể gửi tiếp được, chờ POLLOUT
        return true;   // n == 0, hoặc lỗi -> bỏ qua kết nối
    }
    return true;
}

bool TcpServer::makeResponse(const LoginRequestMsg& req, uint64_t nowMs, LoginResponseMsg& resp)
{
    const std::string name(req.name, strnlen(req.name, sizeof(req.name)));
    const std::string password(req.password, strnlen(req.password, sizeof(req.password)));

    const auto user = users_.find(name);
    if (user == users_.end())
    {
        resp.status = static_cast<uint8_t>(LoginStatus::NAME_TAKEN);
        return false;
    }

    if (user->second != password)
    {
        resp.status = static_cast<uint8_t>(LoginStatus::WRONG_PASSWORD);
        return false;
    }

    const uint32_t playerID = nextPlayerID_++;

    resp.status = static_cast<uint8_t>(LoginStatus::OK);
    resp.playerID = playerID;
    resp.sessionToken = randomToken();
    sessions_[resp.sessionToken] = Session{nowMs};
    resp.udpPort = htons(udpPort_);
    resp.spawnX       = SPAWN_X;
    resp.spawnY       = SPAWN_Y;
    std::printf("login ok: '%s' -> playerID=%u\n", name.c_str(), playerID);
    return true;
}
