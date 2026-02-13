#include "network_manager.h"
#include "gd_example.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>

using namespace godot;

void NetworkManager::_bind_methods() {

}

NetworkManager::NetworkManager() {
    socket = nullptr;
}

NetworkManager::~NetworkManager() {
    if (socket) {
        net_socket_destroy(socket);
    }
}

void NetworkManager::_ready() {
    socket = net_socket_create("127.0.0.1:0");
    if (!socket) {
        UtilityFunctions::print("[Client] Erreur socket !");
        return;
    }

    const char* server_addr = "127.0.0.1:4242";
    uint8_t hello = 1;
    net_socket_send(socket, server_addr, &hello, 1);

    UtilityFunctions::print("[Client] NetworkManager connecté.");
}

void NetworkManager::_process(double delta) {
    if (!socket) return;

    uint8_t buffer[1024];
    char sender[256];

    while (true) {
        int32_t bytes = net_socket_poll(socket, buffer, sizeof(buffer), sender, sizeof(sender));
        if (bytes <= 0) break;

        if (bytes >= sizeof(SpawnPacket)) {
            SpawnPacket* packet = (SpawnPacket*)buffer;

            if (packet->packet_type == 0) { // le type de spawn
                UtilityFunctions::print("[Client] Spawn reçu ID: ", packet->network_id);

                GDExample* new_entity = memnew(GDExample);

                new_entity->set_name("Entity_" + String::num_int64(packet->network_id));
                new_entity->set_position(Vector2(100.0f + (float)(packet->network_id % 10) * 50.0f, 100.0f));

                Ref<Texture2D> texture = ResourceLoader::get_singleton()->load("res://icon.svg");
                new_entity->set_texture(texture);

                add_child(new_entity);
            }
        }
    }
}
