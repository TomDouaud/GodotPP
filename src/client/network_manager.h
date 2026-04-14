#ifndef GODOTPP_NETWORK_MANAGER_H
#define GODOTPP_NETWORK_MANAGER_H

#include <snl.h>
#include <unordered_map>
#include <vector>
#include <godot_cpp/classes/node.hpp>
#include "../../common/protocol.h"

namespace godot {

struct Snapshot {
    uint64_t timestamp;
    float x;
    float y;
};

class NetworkManager : public Node {
     GDCLASS(NetworkManager, Node)

private:
    GameSocket* socket = nullptr;
    uint32_t my_network_id = 0; // Pour stocker l'ID du client local

    uint32_t current_sequence = 0; // numéro de sécquence de l'input packet
    InputState input_history[20]; // Le buffeur Local

    double ping_timer = 0.0;
    uint32_t current_ping_id = 0;

    std::unordered_map<uint32_t, std::vector<Snapshot>> entity_snapshots;
    double estimated_server_time = 0.0;

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