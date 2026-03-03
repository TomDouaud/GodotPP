#include "gd_example.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void GDExample::_bind_methods() {
}

GDExample::GDExample() {
    initial_position = Vector2(0, 0);
}

void GDExample::_ready() {
    initial_position = get_position();
}

GDExample::~GDExample() {
    // Add your cleanup here.
}

void GDExample::_process(double delta) {

}