#ifndef PLATFORM_H
#define PLATFORM_H

// NOTE: Game -> Platform

struct Game_Input {
  bool move_forward, move_back, strafe_left, strafe_right;
  bool rotate_cw, rotate_ccw;
};

struct Debug_Render_Data;

function void                game_init   ();
function void                game_tick   (Game_Input input, f32 dt);
function Debug_Render_Data*  game_render (u16 resolution_width, u16 resolution_height);

// NOTE: Platform -> Game

function void r_set_pixel_debug (Vec2 pos, Vec3 color);
function void r_set_pixel (Vec2 pos, bool dim);

#endif // PLATFORM_H