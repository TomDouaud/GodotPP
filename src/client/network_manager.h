#ifndef GODOTPP_NETWORK_MANAGER_H
#define GODOTPP_NETWORK_MANAGER_H

#include <snl.h>
#include <godot_cpp/classes/node.hpp>

#pragma pack(push, 1)
enum InputFlags : uint8_t {
    INPUT_NONE   = 0,
    INPUT_UP     = 1 << 0, // 1  (0000 0001)
    INPUT_DOWN   = 1 << 1, // 2  (0000 0010)
    INPUT_LEFT   = 1 << 2, // 4  (0000 0100)
    INPUT_RIGHT  = 1 << 3, // 8  (0000 1000)
    INPUT_ACTION = 1 << 4  // 16 (0001 0000)
};

struct SpawnPacket {
    uint32_t packet_type;
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
#pragma pack(pop)

namespace godot {
class NetworkManager : public Node {
     GDCLASS(NetworkManager, Node)

private:
    GameSocket* socket = nullptr;
    uint32_t my_network_id = 0; // Pour stocker l'ID du client local

    uint32_t current_sequence = 0; // numéro de sécquence de l'input packet
    InputState input_history[20]; // Le buffeur Local

    double ping_timer = 0.0;
    uint32_t current_ping_id = 0;

protected:
    static void _bind_methods();

public:
    NetworkManager();
    ~NetworkManager();

    void _ready() override;
    void _process(double delta) override;
    void _exit_tree() override;
};

}
#endif //GODOTPP_NETWORK_MANAGER_H