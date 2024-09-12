function Bitmap* 
r_get_framebuffer (void) {
    local_persist Bitmap f;
    if (f.width == 0) {
        f.width = 640;
        f.height = 360;
        f.pixels = malloc(f.width * f.height * sizeof(u32));
    }
    return &f;
}

function void 
r_put_pixel_at (Vec2 p, Color c);

function void 
r_clear_screen (void);

function void 
r_clear_screen_col_slow (Color c);

function void 
r_draw_circle (Vec2 p, f32 r, Color c);

function void 
r_draw_line (Vec2 p0, Vec2 p1, Color c);

function void 
r_draw_quad_framef (f32 x0, f32 y0, f32 x1, f32 y1, Color c);

function void 
r_draw_quad_frame (Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, Color c);