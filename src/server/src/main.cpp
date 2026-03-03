#include <iostream>
#include <thread>
#include <chrono>

#include <entt/entt.hpp>
#include <snl.h>
#include <unordered_map>
#include <random>

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

    struct PlayerData {
        uint32_t id;
        float x;
        float y;
    };

    std::unordered_map<std::string, PlayerData> connected_clients;

    std::cout << "[Server] Listening..." << std::endl;

    bool running = true;
    while (running) {
        uint8_t buffer[1024];
        char sender_addr[256];

        int32_t bytes_read = net_socket_poll(socket, buffer, sizeof(buffer), sender_addr, sizeof(sender_addr));

        if (bytes_read > 0) {
            std::string client_addr(sender_addr);

            if (connected_clients.find(client_addr) == connected_clients.end()) {

                uint32_t new_id = next_network_id++;
                float spawn_x = 300.0f + (static_cast<float>(rand()) / RAND_MAX) * 500.0f;
                float spawn_y = 200.0f + (static_cast<float>(rand()) / RAND_MAX) * 200.0f;

                connected_clients[client_addr] = {new_id, spawn_x, spawn_y};

                std::cout << "[Server] Spawning ID " << new_id << " at " << spawn_x << ", " << spawn_y << std::endl;

                SpawnPacket new_player_packet;
                new_player_packet.packet_type = PACKET_SPAWN;
                new_player_packet.network_id = new_id;
                new_player_packet.class_id = TYPE_PLAYER;
                new_player_packet.x = spawn_x;
                new_player_packet.y = spawn_y;

                // Brodcast qui manquait lors du commit précédent
                for (const auto& pair : connected_clients) {
                    net_socket_send(socket, pair.first.c_str(), (const uint8_t*)&new_player_packet, sizeof(SpawnPacket));
                }

                // Envoi a un nouveau joueur toutes les infos des joueurs déjà présents, parreil n'etait pas présent au commit précédent
                for (const auto& pair : connected_clients) {
                    if (pair.second.id != new_id) {
                        SpawnPacket old_player_packet;
                        old_player_packet.packet_type = PACKET_SPAWN;
                        old_player_packet.network_id = pair.second.id;
                        old_player_packet.class_id = TYPE_PLAYER;
                        old_player_packet.x = pair.second.x;
                        old_player_packet.y = pair.second.y;
                        net_socket_send(socket, client_addr.c_str(), (const uint8_t*)&old_player_packet, sizeof(SpawnPacket));
                    }
                }
            }
            else {
                // client déja connecté, gestion de son mouvement
                if (buffer[0] == 1 && bytes_read >= sizeof(MovePacket)) {
                    MovePacket* mp = (MovePacket*)buffer;

                    // mise a jour du serveur
                    connected_clients[client_addr].x = mp->x;
                    connected_clients[client_addr].y = mp->y;

                    // BRoadcast aux clients sauf celui qui a envoyé le mouvement
                    for (const auto& pair : connected_clients) {
                        if (pair.first != client_addr) {
                            net_socket_send(socket, pair.first.c_str(), buffer, sizeof(MovePacket));
                        }
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}