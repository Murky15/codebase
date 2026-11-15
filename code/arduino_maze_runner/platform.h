#ifndef PLATFORM_H
#define PLATFORM_H

// NOTE: Game -> Platform

struct Game_Input {
  bool move_forward, move_back, strafe_left, strafe_right;
  bool rotate_cw, rotate_ccw;
};

struct Debug_Render_Data;

function void                game_init   (u16 width, u16 height);
function void                game_tick   (Game_Input input, fix16 dt);
function Debug_Render_Data*  game_render ();

function bool map_index (s16 x, s16 y);

// NOTE: Platform -> Game

#if OS_WINDOWS
# define PGMAccessU8(addr)  ((u8)(*(addr)))
#else
# define PGMAccessU8(addr)  pgm_read_byte_near(addr)
#endif

#if OS_WINDOWS
# define PGMAccessU16(addr) ((u16)(*(addr)))
#else
# define PGMAccessU16(addr) pgm_read_word_near(addr)
#endif

function void r_set_pixel_debug (u16 x, u16 y, Vec3 color);
function void r_set_pixel (u16 x, u16 y, bool dim);

#endif // PLATFORM_H