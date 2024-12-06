#ifndef GAME_H
#define GAME_H

typedef struct Entity {
    Vec2 pos;
    f32 rotation_angle;
    f32 radius;
    u16 curr_sector;
} Entity;

typedef struct Border {
    Vec2 p0, p1;
    Color color;
} Border;

#endif //GAME_H
