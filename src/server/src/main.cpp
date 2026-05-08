#include <iostream>
#include <thread>
#include <chrono>

#include <entt/entt.hpp>
#include <snl.h>
#include <unordered_map>
#include <random>

#include "../../common/protocol.h"

const uint32_t TYPE_PLAYER = 1; // type du player, c'est dégueulasse faudrait faire une enum en vrai

struct CNetworkClient {
    std::string address;
    uint32_t last_processed_sequence = 0;
};

struct CNetworkId {
    uint32_t id;
};

struct CPosition {
    float x;
    float y;
};

entt::entity find_client_by_address(entt::registry& registry, const std::string& address) {
    auto view = registry.view<CNetworkClient>();
    for (auto entity : view) {
        if (view.get<CNetworkClient>(entity).address == address) {
            return entity;
        }
    }
    return entt::null; // Retourne null si le client n'existe pas
}

int main() {
    const char* server_address = "127.0.0.1:4242";
    std::cout << "[Server] Starting on port " << server_address << "..." << std::endl;
    GameSocket* socket = net_socket_create(server_address);

    if (!socket) {
        std::cerr << "[Server] Failed to create socket ! Is the port busy?" << std::endl;
        return 1;
    }

    struct PlayerData {
        uint32_t id;
        float x;
        float y;
        uint32_t last_processed_sequence = 0;
    };

    entt::registry registry;
    uint32_t next_network_id = 100;

    std::cout << "[Server] Listening..." << std::endl;

    bool running = true;
    while (running) {
        uint8_t buffer[1024];
        char sender_addr[256];
        bool state_changed = false;

        while (true) {
            int32_t bytes_read = net_socket_poll(socket, buffer, sizeof(buffer), sender_addr, sizeof(sender_addr));
            if (bytes_read <= 0) break;

                std::string client_addr(sender_addr);

                entt::entity client_entity = find_client_by_address(registry, client_addr);

                if (client_entity == entt::null) {
                    uint32_t new_id = next_network_id++;
                    float spawn_x = 300.0f + (static_cast<float>(rand()) / RAND_MAX) * 500.0f;
                    float spawn_y = 200.0f + (static_cast<float>(rand()) / RAND_MAX) * 200.0f;

                    client_entity = registry.create();
                    // ajouts composants résaux
                    registry.emplace<CNetworkClient>(client_entity, client_addr, 0u); // il faut que ca soit un unsigned int pour entt
                    registry.emplace<CNetworkId>(client_entity, new_id);
                    registry.emplace<CPosition>(client_entity, spawn_x, spawn_y);

                    std::cout << "[Server] Spawning ID " << new_id << " (ECS Entity: " << (uint32_t)client_entity << ")" << std::endl;

                    SpawnPacket new_player_packet;
                    new_player_packet.packet_type = PACKET_SPAWN;
                    new_player_packet.network_id = new_id;
                    new_player_packet.class_id = TYPE_PLAYER;
                    new_player_packet.x = spawn_x;
                    new_player_packet.y = spawn_y;

                    // broadcast
                    auto view = registry.view<CNetworkClient, CNetworkId, CPosition>();
                    for (auto entity : view) {
                        auto& net_client = view.get<CNetworkClient>(entity);
                        auto& net_id = view.get<CNetworkId>(entity);
                        auto& pos = view.get<CPosition>(entity);

                        net_socket_send(socket, net_client.address.c_str(), (const uint8_t*)&new_player_packet, sizeof(SpawnPacket));

                        // Catch-up
                        if (entity != client_entity) {
                            SpawnPacket old_player_packet;
                            old_player_packet.packet_type = PACKET_SPAWN;
                            old_player_packet.network_id = net_id.id;
                            old_player_packet.class_id = TYPE_PLAYER;
                            old_player_packet.x = pos.x;
                            old_player_packet.y = pos.y;
                            net_socket_send(socket, client_addr.c_str(), (const uint8_t*)&old_player_packet, sizeof(SpawnPacket));
                        }
                    }
                } else { // client déja connu
                    if (buffer[0] == PACKET_INPUT && bytes_read >= sizeof(InputPacket)) {
                        InputPacket* ip = (InputPacket*)buffer;

                        auto& net_client = registry.get<CNetworkClient>(client_entity);
                        auto& pos = registry.get<CPosition>(client_entity);

                        if (ip->latest_sequence > net_client.last_processed_sequence) {
                            uint32_t frames_to_process = ip->latest_sequence - net_client.last_processed_sequence;
                            if (frames_to_process > 20) frames_to_process = 20;

                            for (int i = frames_to_process - 1; i >= 0; --i) {
                                uint8_t keys = ip->history[i].keys;
                                float delta = 0.016f;
                                float speed = 300.0f;

                                if (keys & INPUT_UP)    pos.y -= speed * delta;
                                if (keys & INPUT_DOWN)  pos.y += speed * delta;
                                if (keys & INPUT_LEFT)  pos.x -= speed * delta;
                                if (keys & INPUT_RIGHT) pos.x += speed * delta;
                            }

                            net_client.last_processed_sequence = ip->latest_sequence;
                            state_changed = true;
                        }
                    }
                    else if (buffer[0] == PACKET_DESTROY && bytes_read >= sizeof(DestroyPacket)) {
                        DestroyPacket* dp = (DestroyPacket*)buffer;

                        std::cout << "[Server] Client ID " << dp->network_id << " (ECS Entity: " << (uint32_t)client_entity << ") s'est deconnecte." << std::endl;

                        auto view = registry.view<CNetworkClient>();
                        for (auto target_entity : view) {
                            if (target_entity != client_entity) { // ca sert a rien de se renvoyer le message
                                const std::string& target_addr = view.get<CNetworkClient>(target_entity).address;
                                net_socket_send(socket, target_addr.c_str(), buffer, sizeof(DestroyPacket));
                            }
                        }

                        registry.destroy(client_entity);
                    } else if (buffer[0] == PACKET_PING && bytes_read >= sizeof(PingRequest)) {
                        PingRequest* req = (PingRequest*)buffer;

                        PingResponse resp;
                        resp.packet_type = PACKET_PONG;
                        resp.id = req->id;
                        resp.t0 = req->t0;
                        resp.t1 = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch()).count();

                        net_socket_send(socket, sender_addr, (const uint8_t*)&resp, sizeof(PingResponse));
                    }
                }
        } // Fin de lecture
        if (state_changed) {
            uint64_t current_server_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

            // boucle sur entitées qui ont une position et un ID
            auto view = registry.view<CNetworkClient, CNetworkId, CPosition>();

            for (auto entity_to_send : view) {
                MovePacket mp;
                mp.packet_type = PACKET_MOVE;
                mp.network_id = view.get<CNetworkId>(entity_to_send).id;
                mp.x = view.get<CPosition>(entity_to_send).x;
                mp.y = view.get<CPosition>(entity_to_send).y;
                mp.timestamp = current_server_time;

                for (auto target_entity : view) {
                    const std::string& target_addr = view.get<CNetworkClient>(target_entity).address;
                    net_socket_send(socket, target_addr.c_str(), (const uint8_t*)&mp, sizeof(MovePacket));
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}