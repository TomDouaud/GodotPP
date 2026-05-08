#include "network_manager.h"

#include <chrono>
#include <iostream>

#include "gd_example.h"

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/classes/input.hpp>

#include "godot_cpp/variant/callable_custom.hpp"


using namespace godot;

void NetworkManager::_bind_methods() {

}

NetworkManager::NetworkManager() {
    socket = nullptr;
    my_network_id = 0;
    current_sequence = 0;

    // remplissage de l'historique de zéros
    for (int i = 0; i < 20; ++i) {
        input_history[i].keys = 0;
        input_history[i].aim_x = 0.0f;
        input_history[i].aim_y = 0.0f;
    }
}

NetworkManager::~NetworkManager() {
    if (socket) {
        net_socket_destroy(socket);
        socket = nullptr;
    }
}

void NetworkManager::_ready() {
    if (Engine::get_singleton()->is_editor_hint()) { // Pour regler le probleme du client qui se connecte dans l'éditeur
        return;
    }

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
    if (Engine::get_singleton()->is_editor_hint()) { // Pour regler le probleme du client qui se connecte dans l'éditeur
        return;
    }

    if (!socket) return;

    uint8_t buffer[1024];
    char sender[256];

    while (true) {
        int32_t bytes = net_socket_poll(socket, buffer, sizeof(buffer), sender, sizeof(sender));
        if (bytes <= 0) break;

        uint8_t p_type = buffer[0];


        if (p_type == 0 && bytes >= sizeof(SpawnPacket)) {
            SpawnPacket* packet = (SpawnPacket*)buffer;

            if (packet->packet_type == PACKET_SPAWN) {
                if (my_network_id == 0) {
                    my_network_id = packet->network_id;
                    UtilityFunctions::print("[Client] Je suis le client ID: ", my_network_id );
                }

                NodePath path(String("Entity_") + String::num_int64(packet->network_id));
                if (get_node_or_null(path) != nullptr) continue; // Si l'entité existe déjà, on ne la recrée pas

                GDExample* new_entity = memnew(GDExample);
                new_entity->set_name(String("Entity_") + String::num_int64(packet->network_id));
                new_entity->set_position(Vector2(packet->x, packet->y));

                Ref<Texture2D> texture = ResourceLoader::get_singleton()->load("res://icon.svg");
                new_entity->set_texture(texture);

                Label* label = memnew(Label);
                label->set_name("PlayerLabel");
                if (packet->network_id == my_network_id) {
                    label->set_text("Client " + String::num_int64(packet->network_id));
                    label->set_modulate(Color(0, 1, 0)); // Client local vert
                } else {
                    label->set_text("Client " + String::num_int64(packet->network_id));
                    label->set_modulate(Color(1, 0, 0)); // Client distant rouge
                }
                label->set_position(Vector2(-45.0f, -80.0f));
                new_entity->add_child(label);

                add_child(new_entity);
            }
        } else if (p_type == PACKET_MOVE && bytes >= sizeof(MovePacket)) {
            MovePacket* mp = (MovePacket*)buffer;

            if (mp->timestamp > estimated_server_time) {
                estimated_server_time = mp->timestamp;
            }

            if (mp->network_id != my_network_id) { // pour pas interpoler le joueur local

                auto& phys = remote_entities[mp->network_id];

                if (phys.current_position == Vector2(0, 0)) { // pour le spawn
                    phys.current_position = Vector2(mp->x, mp->y);
                }

                phys.target_position = Vector2(mp->x, mp->y);
            }
            else {
                Node* node = get_node_or_null(NodePath(String("Entity_") + String::num_int64(mp->network_id)));
                if (node) {
                    GDExample* entity = Object::cast_to<GDExample>(node);

                    Vector2 corrected_pos = Vector2(mp->x, mp->y);

                    pending_inputs.erase(
                        std::remove_if(pending_inputs.begin(), pending_inputs.end(),
                            [mp](const PendingInput& input) {
                                return input.sequence <= mp->last_processed_sequence;
                            }),
                        pending_inputs.end());

                    float speed = 300.0f;
                    float delta_f = 0.016f;
                    for (const auto& input : pending_inputs) {
                        if (input.keys & INPUT_UP)    corrected_pos.y -= speed * delta_f;
                        if (input.keys & INPUT_DOWN)  corrected_pos.y += speed * delta_f;
                        if (input.keys & INPUT_LEFT)  corrected_pos.x -= speed * delta_f;
                        if (input.keys & INPUT_RIGHT) corrected_pos.x += speed * delta_f;
                    }

                    entity->set_position(corrected_pos);
                }
            }
        } else if (p_type == PACKET_DESTROY && bytes >= sizeof(DestroyPacket)) {
            DestroyPacket* dp = (DestroyPacket*)buffer;

            // Vérification que l'ID du client à supprimer n'est pas celui du client local
            if (dp->network_id != my_network_id) {
                // On cherche son Node et on le supprime
                NodePath path(String("Entity_") + String::num_int64(dp->network_id));
                Node* node_to_delete = get_node_or_null(path);

                if (node_to_delete) {
                    std::cout << "[Client] Suppression du joueur " << dp->network_id << std::endl;
                    node_to_delete->queue_free();
                }
            }
        } else if (p_type == PACKET_PONG && bytes >= sizeof(PingResponse)) {
            PingResponse* resp = (PingResponse*)buffer;

            uint64_t t2 = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch()).count();

            uint64_t rtt = t2 - resp->t0;

            NodePath path(String("Entity_") + String::num_int64(my_network_id) + "/PlayerLabel");
            Label* lbl = Object::cast_to<Label>(get_node_or_null(path));

            if (lbl) {
                lbl->set_text("Client " + String::num_int64(my_network_id) + " (Moi) - Ping: " + String::num_int64(rtt) + " ms");
            }

            UtilityFunctions::print("[Client] Ping #", resp->id, " | RTT = ", rtt, " ms");
        }
    }

    if (my_network_id != 0) {
        Input* input = Input::get_singleton();

        // décalage de l'historique : on décale tout vers la fin pour faire de la place à la nouvelle frame en indice 0
        for (int i = 19; i > 0; --i) {
            input_history[i] = input_history[i - 1];
        }

        // Lecture du clavier et conversion en masque de bits
        uint8_t current_keys = INPUT_NONE;

        // opérateur |= pour ajouter les flags
        if (input->is_action_pressed("ui_up"))    current_keys |= INPUT_UP;
        if (input->is_action_pressed("ui_down"))  current_keys |= INPUT_DOWN;
        if (input->is_action_pressed("ui_left"))  current_keys |= INPUT_LEFT;
        if (input->is_action_pressed("ui_right")) current_keys |= INPUT_RIGHT;
        if (input->is_action_pressed("ui_accept")) current_keys |= INPUT_ACTION;

        // Si besoin futur de la souris
        float current_aim_x = 0.0f;
        float current_aim_y = 0.0f;

        // enregistre l'état de la frame dans l'historique à l'indice 0
        input_history[0].keys = current_keys;
        input_history[0].aim_x = current_aim_x;
        input_history[0].aim_y = current_aim_y;

        current_sequence++;

        if (current_keys != INPUT_NONE) {
            pending_inputs.push_back({current_sequence, current_keys}); // ajout de l'action dans la liste des mouvement fait en prédiction

            Node* my_node = get_node_or_null(NodePath(String("Entity_") + String::num_int64(my_network_id)));
            if (my_node) {
                GDExample* entity = Object::cast_to<GDExample>(my_node);
                Vector2 pos = entity->get_position();
                float speed = 300.0f;
                float delta_f = 0.016f; // Simulation locale

                if (current_keys & INPUT_UP)    pos.y -= speed * delta_f;
                if (current_keys & INPUT_DOWN)  pos.y += speed * delta_f;
                if (current_keys & INPUT_LEFT)  pos.x -= speed * delta_f;
                if (current_keys & INPUT_RIGHT) pos.x += speed * delta_f;

                entity->set_position(pos);
            }
        }

        // On envoie le paquet d'input si dans les 20 dernières frames il y a eu au moins une action (pour éviter d'envoyer des paquets d'input vides)
        bool has_input = false;
        for(int i = 0; i < 20; i++) {
            if(input_history[i].keys != INPUT_NONE) has_input = true;
        }

        if (has_input) {
            InputPacket ip;
            ip.packet_type = PACKET_INPUT;
            ip.network_id = my_network_id;
            ip.latest_sequence = current_sequence;

            for (int i = 0; i < 20; ++i) {
                ip.history[i] = input_history[i];
            }
            net_socket_send(socket, "127.0.0.1:4242", (const uint8_t*)&ip, sizeof(InputPacket));
        }

        ping_timer += delta;

        // 1s
        if (ping_timer >= 1.0) {
            ping_timer = 0.0; // remise a zéro du timer

            PingRequest req;
            req.packet_type = PACKET_PING;
            req.id = current_ping_id++;
            // On note l'heure de départ t0
            req.t0 = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch()).count();

            std::cout << "[Client] Envoi du Ping #" << req.id << " au serveur..." << std::endl;

            net_socket_send(socket, "127.0.0.1:4242", (const uint8_t*)&req, sizeof(PingRequest));
        }
    }

    if (estimated_server_time > 0) {
        estimated_server_time += delta * 1000.0; // conversion en milisecondes

        float k = 150.0f; // Force du ressort
        float c = 2.0f * std::sqrt(k); // Amortissement critique

        for (auto& pair : remote_entities) {
            uint32_t id = pair.first;
            auto& phys = pair.second;

            Vector2 displacement = phys.current_position - phys.target_position; // distance sprite - cible

            // -k*X - c*V
            Vector2 force = (-displacement * k) - (phys.velocity * c);

            phys.velocity += force * delta;
            phys.current_position += phys.velocity * delta;

            Node* node = get_node_or_null(NodePath(String("Entity_") + String::num_int64(id)));
            if (node) {
                Object::cast_to<GDExample>(node)->set_position(phys.current_position);
            }
        }
    }
}

void NetworkManager::_exit_tree() {
    if (Engine::get_singleton()->is_editor_hint()) return;

    if (my_network_id != 0) {
        DestroyPacket dp;
        dp.packet_type = 2; // Type destroy
        dp.network_id = my_network_id;

        // dernier envoi au serveur
        net_socket_send(socket, "127.0.0.1:4242", (const uint8_t*)&dp, sizeof(DestroyPacket));
        std::cout << "[Client] Deconnexion envoyee au serveur." << std::endl;
    }

    net_socket_destroy(socket);
}
