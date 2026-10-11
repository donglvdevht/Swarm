#pragma once
#include <cstdint>

// --- Tick dùng chung giữa client và server ---
// Mỗi input packet server nhận được sẽ được xem là đúng một tick FIXED_DT.
// Client phải gửi input theo đúng nhịp TICK_INTERVAL_MS để giả định này còn đúng.
constexpr float    FIXED_DT          = 0.05f;
constexpr uint64_t TICK_INTERVAL_MS  = 50;

#pragma pack(push, 1)

enum class PacketType : uint8_t {
    PLAYER_INPUT = 1,
    PLAYER_STATE   = 2,
};

struct PacketHeader {
    PacketType type;
    uint32_t   seq;
};

// --- Client → Server ---
struct PlayerInputPacket {
    PacketHeader header;
    uint64_t sessionToken;
    bool up, down, left, right;
    bool attack;
    bool run;
};

// --- Sever → Client ---
struct PlayerStatePacket
{
    PacketHeader header;
    uint32_t playerID;
    float x,y;              // Vị trí chính thức
    float velX, velY;
    uint8_t facing;         // 0=Up, 1=Down, 2=Left, 3=Right
    uint8_t flags;          // bit 0: attacking, bit 1: running...
};

#pragma pack(pop)