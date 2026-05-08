#ifndef GODOTPP_NETWORK_MANAGER_H
#define GODOTPP_NETWORK_MANAGER_H

#include <snl.h>
#include <unordered_map>
#include <vector>
#include <godot_cpp/classes/node.hpp>
#include "../../common/protocol.h"
#include "godot_cpp/variant/vector2.hpp"

namespace godot {

struct RemotePhysics {
    Vector2 current_position;
    Vector2 target_position; // Point destination pour l'interpolation
    Vector2 velocity;        // vitesse générée par le ressort
};

struct PendingInput {
    uint32_t sequence;
    uint8_t keys;
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

    std::unordered_map<uint32_t, RemotePhysics> remote_entities;
    double estimated_server_time = 0.0;

    std::vector<PendingInput> pending_inputs; // Pour la prédiction, les inputs qui attendent le retour serveur

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