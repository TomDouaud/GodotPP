#ifndef GDEXAMPLE_H
#define GDEXAMPLE_H

#include <godot_cpp/classes/sprite2d.hpp>

namespace godot {
    class GDExample : public Sprite2D {
        GDCLASS(GDExample, Sprite2D)

    private:
        double time_passed;
        Vector2 initial_position;
        double radius;
        double speed;

    protected:
        static void _bind_methods();

    public:
        GDExample();
        ~GDExample();

        void _ready() override;
        void _process(double delta) override;
    };
}
#endif