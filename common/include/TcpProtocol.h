#pragma once
#include <cstdint>
#pragma pack(push, 1)
constexpr uint16_t MAX_FRAME_PAYLOAD = 512;
constexpr uint16_t MAX_NAME_LEN = 16;
constexpr uint16_t MAX_CHAT_TEXT_LEN = 256;
constexpr uint16_t PROTOCOL_VERSION = 1;

struct TcpFrameHeader {
    uint16_t length; // Payload size, network byte order
    uint8_t  type;
};


struct LoginRequestMsg {
    char     name[MAX_NAME_LEN];
    char     password[MAX_NAME_LEN];
};

struct LoginResponseMsg {
    uint8_t  status;                   // LoginStatus
    uint32_t playerID;
    uint64_t sessionToken;
    uint16_t udpPort;
    float    spawnX, spawnY;
};

struct ChatMessagePayload {
    uint32_t playerID; // Network byte order; assigned by the server
    char     sender[MAX_NAME_LEN];
    char     text[MAX_CHAT_TEXT_LEN];
};
#pragma pack(pop)

enum class LoginStatus : uint8_t {
    OK = 0,
    WRONG_PASSWORD,
    NAME_TAKEN,
    SERVER_FULL,
    BAD_VERSION,
};

enum class TcpMsgType : uint8_t {
    LOGIN_REQUEST  = 1,
    LOGIN_RESPONSE = 2,
    CHAT_MESSAGE   = 3,
};
