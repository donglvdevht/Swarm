#pragma once
#include <cstdint>
#pragma pack(push, 1)

enum class PacketType : uint8_t {
    PLAYER_INPUT = 1,
    ECHO_REPLY   = 2,
};

struct PacketHeader {
    PacketType type;
    uint32_t   tick;
};

struct PlayerInputPacket {
    PacketHeader header;
    bool up, down, left, right;
    bool attack;
    bool run;
};

struct EchoReplyPagket
{
    PacketHeader header;
};

#pragma pack(pop)