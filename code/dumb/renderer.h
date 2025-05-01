#ifndef RENDERER_H
#define RENDERER_H

#define ASPECT_W 16.f
#define ASPECT_H 9.f

#define RESOLUTION_W 320 // 640
#define RESOLUTION_H 180 // 360
#define TEXTURE_VERT_REPEAT_SCALE 1.5

#define MAX_COLUMNS RESOLUTION_W
#define MAX_SPANS   RESOLUTION_H

#define MAX_ITERATIONS (s32_max - 1)
#define MAX_WALLS_IN_SECTOR 64

typedef struct Bitmap {
    u32 *pixels;
    u32 width, height;
} Bitmap;

typedef struct Edge {
    Vec2 minp, maxp;
    f32 recslope;
} Edge;

//- @note: Fundementals
function Bitmap* r_get_framebuffer(void);
function void r_test_gradient(void);
function void r_put_pixel_at(Vec2 p, Color c);
function void r_clear(void);
function void r_clear_color(Color c);

//- @note: Primitives
function void r_draw_circle(Vec2 p, f32 r, Color c);
function void r_draw_line(Vec2 p0, Vec2 p1, Color c);
function void r_draw_vert(f32 x, f32 y0, f32 y1, Color c);
function void r_draw_vert_textured (f32 x, f32 y0, f32 y1, f32 actual_height, PNG_Bitmap_RGBA texture, Texture_Map_Type map_type, s32 texx);
function void r_draw_quad_framef(f32 x0, f32 y0, f32 x1, f32 y1, Color c);
function void r_draw_quad_frame(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, Color c);
function void r_draw_rect(Vec2 p, Vec2 sz, Color c);

//- @note: Game specific
function Edge r_make_edge(Vec2 minp, Vec2 maxp);
function void r_sector(Map *map, Sector *sector, Asset_Group environment_textures, Entity *cam, s32 last_sector, s32 num_iterations, Range window);
function void r_map(Map map, Vec3 map_cam, Entity player, b32 show_player);

#endif //RENDERER_H
