#ifndef RENDERER_H
#define RENDERER_H

// This might be the worst software renderer ever

typedef struct Bitmap {
    u32 *pixels;
    u32 width, height;
} Bitmap;

//- @note: Fundementals
function Bitmap* r_get_framebuffer(void);
function void r_test_gradient(void);
function void r_put_pixel_at(Vec2 p, Color c);
function void r_clear(void);
function void r_clear_color(Color c);

//- @note: Primatives
function void r_draw_circle(Vec2 p, f32 r, Color c);
function void r_draw_line(Vec2 p0, Vec2 p1, Color c);
function void r_draw_vert(f32 x, f32 y0, f32 y1, Color c);
function void r_draw_quad_framef(f32 x0, f32 y0, f32 x1, f32 y1, Color c);
function void r_draw_quad_frame(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, Color c);
function void r_draw_rect(Vec2 p, Vec2 sz, Color c);

//- @note: Game specific functionality
function void r_scene(Vec2 cam_pos, f32 cam_orientation, Border *walls, u64 num_walls);
function void r_map(b32 show_player, Vec2 player_pos, f32 orientation, Border *walls, u64 num_walls);

#endif //RENDERER_H
