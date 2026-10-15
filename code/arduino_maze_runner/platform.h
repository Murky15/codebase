#ifndef PLATFORM_H
#define PLATFORM_H
extern "C" {

// NOTE: Game -> Platform

#define RAYCAST_FOV (M_PI/3.f)
#define RAYCAST_RESOLUTION 32

struct Game_Input {
  bool move_forward, move_back, strafe_left, strafe_right;
  bool rotate_cw, rotate_ccw;
};

struct Debug_Render_Data {
  struct {
    f32 angle, length;
  } rays[RAYCAST_RESOLUTION];
};

function void               game_init   (void *memory, u64 size);
function void               game_tick   (Game_Input input, f32 dt);
function Debug_Render_Data  game_render (u64 resolution_width, u64 resolution_height);

// NOTE: Platform -> Game

function void r_set_pixel (Vec2 pos, Vec3 color);

}
#endif // PLATFORM_H