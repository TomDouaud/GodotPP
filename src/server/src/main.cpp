#include <iostream>
#include <thread>
#include <chrono>

#include <entt/entt.hpp>
#include <snl.h>

struct SpawnPacket {
    uint32_t packet_type;
    uint32_t network_id;
    uint32_t class_id;
};

const uint8_t PACKET_SPAWN = 0; // type du packet de spawn, c'est dégueulasse faudrait faire une enum en vrai
const uint32_t TYPE_PLAYER = 1; // type du player, c'est dégueulasse faudrait faire une enum en vrai

int main() {
    const char* server_address = "127.0.0.1:4242";
    std::cout << "[Server] Starting on port " << server_address << "..." << std::endl;
    GameSocket* socket = net_socket_create(server_address);

    if (!socket) {
        std::cerr << "[Server] Failed to create socket ! Is the port busy?" << std::endl;
        return 1;
    }

    uint32_t next_network_id = 100;

    std::vector<std::string> connected_clients;

    std::cout << "[Server] Listening..." << std::endl;

    bool running = true;
    while (running) {
        uint8_t buffer[1024];
        char sender_addr[256];

        int32_t bytes_read = net_socket_poll(socket, buffer, sizeof(buffer), sender_addr, sizeof(sender_addr));

        if (bytes_read > 0) {
            std::string client_id(sender_addr);

            auto it = std::find(connected_clients.begin(), connected_clients.end(), client_id);

            bool is_new_client = (it == connected_clients.end());

            if (is_new_client) {
                std::cout << "[Server] New connection from: " << client_id << std::endl;

                connected_clients.push_back(client_id);

                uint32_t my_id = next_network_id++;
                std::cout << "[Server] Spawning Entity ID " << my_id << std::endl;

                SpawnPacket packet;
                packet.packet_type = PACKET_SPAWN;
                packet.network_id = my_id;
                packet.class_id = TYPE_PLAYER;

                net_socket_send(socket, sender_addr, (const uint8_t*)&packet, sizeof(packet));
            } else {

            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    net_socket_destroy(socket);
    return 0;
}