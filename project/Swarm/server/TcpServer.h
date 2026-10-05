#pragma once
#include <poll.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "TcpProtocol.h"
#include "Database.h"

// Server login TCP, non-blocking, dùng chung vòng poll() với UDP trong main.
// Mỗi kết nối: nhận LoginRequestMsg -> trả LoginResponseMsg -> đóng.
class TcpServer
{
public:
    ~TcpServer();

    bool start(uint16_t port, const std::string& usersPath, uint16_t udpPort);

    // Thêm listen socket + các kết nối đang mở vào danh sách poll.
    void appendPollFds(std::vector<pollfd>& fds) const;
    // Xử lý revents của các pollfd từ vị trí `first` trở đi.
    void handleEvents(const std::vector<pollfd>& fds, size_t first, uint64_t nowMs);
    // Đóng kết nối login treo quá LOGIN_TIMEOUT_MS.
    void sweep(uint64_t nowMs);
    bool authenticateSession(uint64_t token, uint64_t nowMs);

private:
    struct Conn
    {
        int      fd = -1;
        uint64_t startMs = 0;       // Để kiểm tra timeout
        char     rx[sizeof(LoginRequestMsg)]{};
        size_t   rxGot = 0;
        std::vector<char> tx;       // response, rỗng nghĩa là chưa có
        size_t   txSent = 0;
        bool     replied = false;
    };

    struct Session
    {
        uint64_t lastSeenMs = 0;
    };

    static constexpr size_t   MAX_CONNS        = 64;
    static constexpr uint64_t LOGIN_TIMEOUT_MS = 5000;
    static constexpr uint64_t SESSION_TIMEOUT_MS = 10000;

    int      listenFd_ = -1;
    uint16_t udpPort_  = 0;
    uint32_t nextPlayerID_ = 1;

    std::unordered_map<uint64_t, Session> sessions_;
    std::vector<Conn> conns_;
    Database db;

    bool loadUsers(const std::string& path);
    void acceptAll(uint64_t nowMs);
    Conn* findConn(int fd);
    void closeConn(int fd);

    bool serve(Conn& c, short revents, uint64_t nowMs);   // false = đóng kết nối này
    bool readRequest(Conn& c);            // false = peer đóng / lỗi
    bool flush(Conn& c);                  // true  = gửi xong hoặc lỗi
    bool makeResponse(const LoginRequestMsg& req, uint64_t nowMs,
                      LoginResponseMsg& resp);
};
