#pragma once
#include "Protocol.h"
#include <string>
#include <thread>
#include <atomic>
#include <cstdint>
#include <mutex>

struct ServerState {
    uint32_t lastProcessedSeq = 0;
    float x = 0, y = 0;
    float velX = 0, velY = 0;
    uint8_t facing = 1;
    bool valid = false;
};

class Network {
public:
    Network();
    ~Network();

    // --- Khởi động socket ---
    bool start(const std::string& ip, uint16_t port);
    // --- Dừng socket ---
    void stop();

    void updateInput(bool up, bool down, bool left, bool right, bool attack, bool run);

    ServerState getSeverState();

    uint32_t getCurrentSeq();
private:
    int sock_ = -1;
    std::thread thread_;
    std::atomic<bool> running_{false};
    void networkLoop();

    std::mutex inputMutex_;

    std::mutex stateMutex_;
    ServerState latestState_;

    struct InputSnapshot{
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        bool attack = false;
        bool run = false;
    };

    InputSnapshot currentInput_;
    uint64_t SEND_INTERVAL_MS = 50;
    std::atomic<uint32_t> nextSeq_{0};
};