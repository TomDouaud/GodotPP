#ifndef GODOTPP_NETWORK_MANAGER_H
#define GODOTPP_NETWORK_MANAGER_H

#include <snl.h>
#include <godot_cpp/classes/node.hpp>

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

namespace godot {
class NetworkManager : public Node {
     GDCLASS(NetworkManager, Node)

private:
    GameSocket* socket = nullptr;
    uint32_t my_network_id = 0; // Pour stocker l'ID du client local

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