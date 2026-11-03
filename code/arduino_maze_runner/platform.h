#ifndef PLATFORM_H
#define PLATFORM_H

// NOTE: Game -> Platform

struct Game_Input {
  bool move_forward, move_back, strafe_left, strafe_right;
  bool rotate_cw, rotate_ccw;
};

struct Debug_Render_Data;

function void                game_init   (u16 width, u16 height);
function void                game_tick   (Game_Input input, u16 dt);
function Debug_Render_Data*  game_render ();

// NOTE: Platform -> Game

function void r_set_pixel_debug (u16 x, u16 y, Vec3 color);
function void r_set_pixel (u16 x, u16 y, bool dim);

#endif // PLATFORM_H