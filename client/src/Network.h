#pragma once
#include "Protocol.h"
#include "TcpProtocol.h"
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>
#include <deque>
#include <sys/types.h>

struct ServerState {
    uint32_t lastProcessedSeq = 0;
    float x = 0, y = 0;
    float velX = 0, velY = 0;
    uint8_t facing = 1;
    bool valid = false;
};

struct SentInput {
    uint32_t seq;
    bool up, down, left, right, run;
};

constexpr uint8_t LOGIN_NET_ERROR = 255;   // lỗi mạng/timeout, không phải LoginStatus từ server

enum class ConnState : uint8_t {
    Disconnected,
    TcpConnecting,   // đã gọi connect(), chờ socket writable
    TcpSending,      // đang đẩy LoginRequestMsg
    TcpReceiving,    // đang gom LoginResponseMsg
    Connected,
    Failed
};

class Network {
public:
    Network();
    ~Network();

    // --- Khởi động socket ---
    bool start(const std::string& ip, uint16_t port);
    void startTcpConnect(const std::string& strIp, uint16_t uPort, uint64_t nowTime);

    // --- Dừng socket ---
    void stop();

    void beginLogin(const std::string& name, const std::string& pass);
    void failLogin();
    void updateInput(bool up, bool down, bool left, bool right, bool attack, bool run);
    ServerState getSeverState();
    ConnState getConnState()  const { return connState_.load(); }
    uint8_t   getLastStatus() const { return lastStatus_.load(); }

    // Các input đã gửi nhưng server chưa xác nhận (seq > ackSeq), dùng cho reconciliation
    std::vector<SentInput> getUnackedInputs(uint32_t ackSeq);
private:
    int sock_ = -1;
    int tcpSock_ = -1;
    std::string serverIp_;
    uint16_t fallbackUdpPort_ = 0;

    std::thread thread_;
    ServerState latestState_;
    uint64_t lastReceivedTime_ = 0;
    uint64_t loginStartMs_ = 0;
    std::string pendingName_, pendingPass_;
    char rxBuf_[sizeof(LoginResponseMsg)];  size_t rxGot_ = 0;
    std::vector<char> txBuf_;   size_t txSent_ = 0;

    uint32_t playerID_;
    uint64_t sessionToken_ = 0;
    uint16_t udpPort_;

    std::atomic<bool> running_{false};
    std::atomic<bool> loginRequested_{false};
    std::atomic<ConnState> connState_{ConnState::Disconnected};
    std::atomic<uint8_t> lastStatus_{0};

    std::mutex inputMutex_;
    std::mutex stateMutex_;
    std::mutex loginMutex_;

    static constexpr uint64_t SERVER_TIMEOUT_MS = 3000;
    static constexpr uint64_t LOGIN_TIMEOUT_MS = 5000;

    void networkLoop();
    void handlePacket(const char* buffer, ssize_t n);
    void checkServerTimeout(uint64_t nowTime);
    void pumpLogin(uint64_t nowTime);
    void buildRequest();
    void finishLogin();
    bool connectUdp(uint16_t port);
    void closeTcp();

    struct InputSnapshot{
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        bool attack = false;
        bool run = false;
    };

    InputSnapshot currentInput_;
    InputSnapshot pendingSendInput_;
    std::atomic<uint32_t> nextSeq_{0};
    std::deque<SentInput> sentHistory_;   // bảo vệ bởi inputMutex_
};
