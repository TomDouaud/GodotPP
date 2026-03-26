#include <iostream>
#include <thread>
#include <chrono>

#include <entt/entt.hpp>
#include <snl.h>
#include <unordered_map>
#include <random>

#include "../../common/protocol.h"

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
        uint32_t last_processed_sequence = 0;
    };

    std::unordered_map<std::string, PlayerData> connected_clients;

    std::cout << "[Server] Listening..." << std::endl;

    bool running = true;
    while (running) {
        uint8_t buffer[1024];
        char sender_addr[256];
        bool state_changed = false;

        while (true) {
            int32_t bytes_read = net_socket_poll(socket, buffer, sizeof(buffer), sender_addr, sizeof(sender_addr));

            if (bytes_read <= 0) {
                break;
            }

            std::string client_addr(sender_addr);

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
                    if (buffer[0] == PACKET_MOVE && bytes_read >= sizeof(MovePacket)) {

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
                    else if (buffer[0] == PACKET_DESTROY && bytes_read >= sizeof(DestroyPacket)) {
                        DestroyPacket* dp = (DestroyPacket*)buffer;

                        std::cout << "[Server] Client ID " << dp->network_id << " s'est deconnecte." << std::endl;

                        // Prévenir a tout les autres joueurs la déconnexion
                        for (const auto& pair : connected_clients) {
                            if (pair.first != client_addr) {
                                net_socket_send(socket, pair.first.c_str(), buffer, sizeof(DestroyPacket));
                            }
                        }
                        // Supression du client dans la liste des clients connectés
                        connected_clients.erase(client_addr);
                    }
                    else if (buffer[0] == PACKET_INPUT && bytes_read >= sizeof(InputPacket)) {
                        InputPacket* ip = (InputPacket*)buffer;

                        // récupération du joueur et utilisation du & pour éviter de faire une copie et de pouvoir mettre à jour directement les données du joueur
                        auto& player = connected_clients[client_addr];

                        // Pour éviter de traiter les frames déja traitées
                        if (ip->latest_sequence > player.last_processed_sequence) {

                            // Vérification du nombre de frames à traiter
                            uint32_t frames_to_process = ip->latest_sequence - player.last_processed_sequence;

                            // Sécurité pour éviter de traiter trop de frames d'un coup si le client a eu un lag ou a envoyé un paquet d'input très vieux
                            if (frames_to_process > 20) frames_to_process = 20;

                            // Application des mouvements de facon chronlogique
                            for (int i = frames_to_process - 1; i >= 0; --i) {
                                uint8_t keys = ip->history[i].keys;

                                // Simulation de la physique du serveur a 60fps
                                float delta = 0.016f;
                                float speed = 300.0f;

                                // Opérateurs bit-à-bit (&) pour vérifier chaque touche
                                if (keys & INPUT_UP)    player.y -= speed * delta;
                                if (keys & INPUT_DOWN)  player.y += speed * delta;
                                if (keys & INPUT_LEFT)  player.x -= speed * delta;
                                if (keys & INPUT_RIGHT) player.x += speed * delta;
                            }

                            // mise a jour du numéro de séquence traité pour ce joueur
                            player.last_processed_sequence = ip->latest_sequence;

                            // Broadcast
                            MovePacket mp;
                            mp.packet_type = PACKET_MOVE;
                            mp.network_id = player.id;
                            mp.x = player.x;
                            mp.y = player.y;

                            for (const auto& pair : connected_clients) {
                                net_socket_send(socket, pair.first.c_str(), (const uint8_t*)&mp, sizeof(MovePacket));
                            }
                        }
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
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}