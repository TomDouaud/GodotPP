#include "network_manager.h"

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

            if (packet->packet_type == 0) { // le type de spawn
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
                if (packet->network_id == my_network_id) {
                    label->set_text("Client " + String::num_int64(packet->network_id) + " (Client Local)");
                    label->set_modulate(Color(0, 1, 0)); // Client local vert
                } else {
                    label->set_text("Client " + String::num_int64(packet->network_id));
                    label->set_modulate(Color(1, 0, 0)); // Client distant rouge
                }
                label->set_position(Vector2(-45.0f, -80.0f));
                new_entity->add_child(label);

                add_child(new_entity);
            }
        } else if (p_type == 1 && bytes >= sizeof(MovePacket)) {
            MovePacket* mp = (MovePacket*)buffer;

            // Si le mouvement vient d'un autre client, on met à jour sa position
            if (mp->network_id != my_network_id) {
                Node* node = get_node_or_null(NodePath(String("Entity_") + String::num_int64(mp->network_id)));
                if (node) {
                    GDExample* entity = Object::cast_to<GDExample>(node);
                    if (entity) {
                        entity->set_position(Vector2(mp->x, mp->y));
                    }
                }
            }
        } else if (p_type == 2 && bytes >= sizeof(DestroyPacket)) {
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
        }
    }

    if (my_network_id != 0) {
        Input* input = Input::get_singleton();
        Vector2 velocity(0, 0);

        // Pour l'instant vu que c'est du test pour voir si le mouvement marche, j'ai hardcodé les touches de déplacement ici
        if (input->is_action_pressed("ui_right")) velocity.x += 1;
        if (input->is_action_pressed("ui_left"))  velocity.x -= 1;
        if (input->is_action_pressed("ui_down"))  velocity.y += 1;
        if (input->is_action_pressed("ui_up"))    velocity.y -= 1;

        if (velocity.length() > 0) {
            Node* my_node = get_node_or_null(NodePath(String("Entity_") + String::num_int64(my_network_id)));
            if (my_node) {
                GDExample* my_player = Object::cast_to<GDExample>(my_node);
                if (my_player) {
                    Vector2 new_pos = my_player->get_position() + (velocity.normalized() * 300.0f * delta);
                    my_player->set_position(new_pos);

                    MovePacket mp;
                    mp.packet_type = 1;
                    mp.network_id = my_network_id;
                    mp.x = new_pos.x;
                    mp.y = new_pos.y;
                    net_socket_send(socket, "127.0.0.1:4242", (const uint8_t*)&mp, sizeof(MovePacket)); // renvoi aux autres client le mouvement du client local
                }
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
