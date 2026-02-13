#include "gd_example.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void GDExample::_bind_methods() {
}

GDExample::GDExample() {
    time_passed = 0.0;
    radius = 10.0;
    speed = 1.0;
    initial_position = Vector2(0, 0);
}

void GDExample::_ready() {
    initial_position = get_position();
}

GDExample::~GDExample() {
    // Add your cleanup here.
}

void GDExample::_process(double delta) {
    time_passed += delta;

    Vector2 offset = Vector2(
        static_cast<float>(radius * std::cos(time_passed * speed)),
        static_cast<float>(radius * std::sin(time_passed * speed))
    );

    Vector2 new_position = initial_position + offset;
    set_position(new_position);
}