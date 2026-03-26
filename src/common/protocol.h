//
// Created by douau on 26/03/2026.
//

#ifndef GODOTPP_PROTOCOL_H
#define GODOTPP_PROTOCOL_H


enum InputFlags : uint8_t {
    INPUT_NONE   = 0,
    INPUT_UP     = 1 << 0, // 1  (0000 0001)
    INPUT_DOWN   = 1 << 1, // 2  (0000 0010)
    INPUT_LEFT   = 1 << 2, // 4  (0000 0100)
    INPUT_RIGHT  = 1 << 3, // 8  (0000 1000)
    INPUT_ACTION = 1 << 4  // 16 (0001 0000)
};

#pragma pack(push, 1)

struct SpawnPacket {
    uint32_t packet_type; // de valeure 0
    uint32_t network_id;
    uint32_t class_id;
    float x;
    float y;
};

struct MovePacket {
    uint8_t packet_type; // de valeure 1
    uint32_t network_id;
    float x;
    float y;
};

struct DestroyPacket {
    uint8_t packet_type; // de valeure 2
    uint32_t network_id;
};

// l'état d'une seule frame
struct InputState {
    uint8_t keys;
    float aim_x;
    float aim_y;
};

struct InputPacket {
    uint8_t packet_type;       // de valeure 3
    uint32_t network_id;       // L'ID du joueur qui envoie les inputs
    uint32_t latest_sequence;  // Le numéro de séquence augmantant a chaque frame
    InputState history[20];    // 20 dernieres frames
};

struct PingRequest {
    uint8_t packet_type; // de valeure 4
    uint32_t id;         // Identifiant
    uint64_t t0;         // Timestamp du client à l'envoi
};

struct PingResponse {
    uint8_t packet_type; // de valeure 5
    uint32_t id;
    uint64_t t0;         // Recopié depuis le ping
    uint64_t t1;         // Timestamp du serveur à l'envoi
};

enum PacketType : uint8_t {
    PACKET_SPAWN = 0,
    PACKET_MOVE = 1,
    PACKET_DESTROY = 2,
    PACKET_INPUT = 3,
    PACKET_PING = 4,
    PACKET_PONG = 5
};
#pragma pack(pop)

#endif //GODOTPP_PROTOCOL_H