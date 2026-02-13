#ifndef GODOTPP_NETWORK_MANAGER_H
#define GODOTPP_NETWORK_MANAGER_H

#include <godot_cpp/classes/nodes>
#include <snl.h>

struct SpawnPacket {
    uint32_t packet_type;
    uint32_t network_id;
    uint32_t class_id;
};

namespace godot {
class NetworkManager : public Node {
     GDCLASS(NetworkManager, Node)

private:
    GameSocket* socket = nullptr;

protected:
    static void _bind_methods();

public:
    NetworkManager();
    ~NetworkManager();

    void _ready() override;
    void _process(double delta) override;
};

}
#endif //GODOTPP_NETWORK_MANAGER_H