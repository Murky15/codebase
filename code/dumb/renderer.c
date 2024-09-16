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
r_test_gradient (void) {
    Bitmap *canvas = r_get_framebuffer();
    
    local_persist u8 xOffset, yOffset;
    for (u32 y = 0; y < canvas->height; ++y) {
        for (u32 x = 0; x < canvas->width; ++x) {
            u8 r = (u8)x + xOffset;
            u8 g = (u8)y + yOffset;
            u8 b = 0;
            canvas->pixels[y * canvas->width + x] = (r << 16 | g << 8 | b);
        }
    }
    xOffset = ++yOffset;
}

function void 
r_put_pixel_at (Vec2 p, Color c) {
    Bitmap *canvas = r_get_framebuffer();
    
    Vec2i pi = v2i_from_v2(p);
    if (pi.x >= 0 && pi.y >= 0 && pi.x < canvas->width && pi.y < canvas->height)
        canvas->pixels[pi.y * canvas->width + pi.x] = (c.r << 16 | c.g << 8 | c.b);
}

function void 
r_clear (void) {
    Bitmap *canvas = r_get_framebuffer();
    memory_zero(canvas->pixels, canvas->width * canvas->height * sizeof(u32));
}

// @slow
function void 
r_clear_color (Color c) {
    Bitmap *canvas = r_get_framebuffer();
    for (u32 pidx = 0; pidx < canvas->width * canvas->height; ++pidx) {
        canvas->pixels[pidx] = (c.r << 16 | c.g << 8 | c.b);
    }
}

function void 
r_draw_circle (Vec2 p, f32 r, Color c) {
    for (f32 y = -r; y <= r; ++y) {
        for (f32 x = -r; x <= r; ++x) {
            if (sq(x) + sq(y) <= sq(r))
                r_put_pixel_at(v2(x + p.x, y + p.y), c);
        }
    }
}

function void
r_impl_draw_line_low (Vec2 p0, Vec2 p1, Color c) {
    f32 dx = p1.x - p0.x;
    f32 dy = p1.y - p0.y;
    f32 yi = 1.f;
    if (dy < 0) {
        yi = -1.f;
        dy = -dy;
    }
    f32 d = 2.f*dy - dx;
    f32 y = p0.y;
    
    for (f32 x = p0.x; x <= p1.x; ++x) {
        r_put_pixel_at(v2(x,y), c);
        if (d > 0) {
            y += yi;
            d += (2.f * (dy - dx));
        } else {
            d += 2.f * dy;
        }
    }
}

function void
r_impl_draw_line_high (Vec2 p0, Vec2 p1, Color c) {
    f32 dx = p1.x - p0.x;
    f32 dy = p1.y - p0.y;
    f32 xi = 1.f;
    if (dx < 0) {
        xi = -1.f;
        dx = -dx;
    }
    f32 d = 2.f*dx - dy;
    f32 x = p0.x;
    
    for (f32 y = p0.y; y <= p1.y; ++y) {
        r_put_pixel_at(v2(x,y), c);
        if (d > 0) {
            x += xi;
            d += (2.f * (dx - dy));
        } else {
            d += 2.f * dx;
        }
    }
}

function void 
r_draw_line (Vec2 p0, Vec2 p1, Color c) {
    if (fabs(p1.y - p0.y) < fabs(p1.x - p0.x)) {
        if (p0.x > p1.x)
            r_impl_draw_line_low(p1, p0, c);
        else
            r_impl_draw_line_low(p0, p1, c);
    } else {
        if (p0.y > p1.y)
            r_impl_draw_line_high(p1, p0, c);
        else
            r_impl_draw_line_high(p0, p1, c);
    }
}

function void
r_draw_vert (f32 x, f32 y0, f32 y1, Color c) {
    //assert(y0 <= y1);
    for (f32 y = y0; y <= y1; ++y)
        r_put_pixel_at(v2(x,y), c);
}

function void 
r_draw_quad_framef (f32 x0, f32 y0, f32 x1, f32 y1, Color c) {
    r_draw_line(v2(x0, y0), v2(x1, y0), c);
    r_draw_line(v2(x1, y0), v2(x1, y1), c);
    r_draw_line(v2(x1, y1), v2(x0, y1), c);
    r_draw_line(v2(x0, y1), v2(x0, y0), c);
}

function void 
r_draw_quad_frame (Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, Color c) {
    r_draw_line(p0, p1, c);
    r_draw_line(p1, p2, c);
    r_draw_line(p2, p3, c);
    r_draw_line(p3, p0, c);
}

// @slow
function void 
r_scene (Vec2 cam_pos, f32 cam_orientation, Wall *walls, u64 num_walls) {
    Bitmap *canvas = r_get_framebuffer();
    
    //- @note: Constants
    local_persist read_only f32 forward = M_PI32 / 2.f;
    local_persist read_only f32 cam_dist = 1.f; // @todo: Have this be changable
    f32 width_middle = canvas->width/2.f;
    f32 height_middle = canvas->height/2.f;
    f32 near_plane = 10;
    
    for (u64 wall_idx = 0; wall_idx < num_walls; ++wall_idx) {
        //- @note: Transform wall relative to player
        Vec2 t0 = v2sub(walls[wall_idx].p0, cam_pos);
        Vec2 t1 = v2sub(walls[wall_idx].p1, cam_pos);
        
        f32 t = -cam_orientation + forward;
        Vec2 d0, d1;
        d0.x = t0.x * cosf(t) - t0.y * sinf(t);
        d0.y = t0.x * sinf(t) + t0.y * cosf(t);
        d1.x = t1.x * cosf(t) - t1.y * sinf(t);
        d1.y = t1.x * sinf(t) + t1.y * cosf(t);
        
        //- @note: Clip walls behind player
        f32 z0 = d0.y;
        f32 z1 = d1.y;
        if (z0 < near_plane && z1 < near_plane) 
            continue;
        
        f32 clipped_x = d0.x + (((d1.x - d0.x) * (near_plane - d0.y)) / (d1.y - d0.y));
        if (z0 < near_plane) {
            d0 = v2(clipped_x, near_plane);
            z0 = near_plane;
        } else if (z1 < 0) {
            d1 = v2(clipped_x, near_plane);
            z1 = near_plane;
        }
        
        f32 p0 = d0.x / z0;
        f32 p1 = d1.x / z1;
        
        f32 s0 = width_middle + p0 * 80.f;
        f32 s1 = width_middle + p1 * 80.f;
        r_draw_line(v2(s0, height_middle), v2(s1, height_middle), walls[wall_idx].color);
        
#if 1
        //- @note: Minimap
#define MINIMAP_SCALE 0.3f
        Vec2 fixed_player = v2(width_middle, height_middle);
        f32 player_radius = 10.f * MINIMAP_SCALE;
        f32 turn_indicator_length = 20.f * MINIMAP_SCALE;
        Vec2 player_minimap_pos = v2muls(fixed_player, MINIMAP_SCALE);
        r_draw_circle(player_minimap_pos, player_radius, Color_White);
        r_draw_line(player_minimap_pos,  v2add(player_minimap_pos, v2(cosf(forward) * turn_indicator_length, sinf(forward) * turn_indicator_length)), Color_Magenta);
        r_draw_line(v2muls(v2add(v2(-width_middle, 0), fixed_player), MINIMAP_SCALE), v2muls(v2add(v2(width_middle, 0), fixed_player), MINIMAP_SCALE), Color_Lime);
        
        r_draw_line(v2muls(v2add(d0, fixed_player), MINIMAP_SCALE), v2muls(v2add(d1, fixed_player), MINIMAP_SCALE), Color_Red);
        
        r_draw_quad_framef(0, 0, canvas->width * MINIMAP_SCALE, canvas->height * MINIMAP_SCALE, Color_Blue);
#endif
    }
}


