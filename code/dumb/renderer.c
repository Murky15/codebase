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
    //-
    
    for (u64 wall_idx = 0; wall_idx < num_walls; ++wall_idx) {
        //- @note: Transform wall relative to player
        Vec2 t0 = v2sub(walls[wall_idx].p0, cam_pos);
        Vec2 t1 = v2sub(walls[wall_idx].p1, cam_pos);
        
#define rotate_x(p,t) (((p).x*cosf(t))-((p).y*sinf(t)))
#define rotate_y(p,t) (((p).x*sinf(t))+((p).y*cosf(t)))
        f32 t = -cam_orientation + forward;
        Vec2 d0, d1;
        d0.x = rotate_x(t0,t);
        d0.y = rotate_y(t0,t);
        d1.x = rotate_x(t1,t);
        d1.y = rotate_y(t1,t);
#undef rotate_x
#undef rotate_y
        
        //- @note: Clip walls behind player
        Vec2 vpp[2] = {{-width_middle,1},{width_middle,1}};
        f32 v0 = v2cross(vpp[0], vpp[1], d0);
        f32 v1 = v2cross(vpp[0], vpp[1], d1);
        if (v0 < 0 && v1 < 0)
            continue; // Wall is completely behind player
        
        Vec3 vp = v3norm(v3cross(pv2(vpp[0],1), pv2(vpp[1],1)));
        Vec3 line = v3norm(v3cross(pv2(d0,1), pv2(d1,1)));
        Vec3 intersection = v3cross(vp, line);
        if (intersection.z != 0) {
            Vec2 i = v2(intersection.x / intersection.z, intersection.y / intersection.z);
            f32 minx = min(d0.x, d1.x);
            f32 maxx = max(d0.x, d1.x);
            f32 miny = min(d0.y, d1.y);
            f32 maxy = max(d0.y, d1.y);
            
            // Check if intersection is on line segment & clip walls accordingly
            if (minx <= i.x && i.x <= maxx && miny <= i.y && i.y <= maxy) {
                if (v0 < 0) 
                    d0 = i;
                else if (v1 < 0)
                    d1 = i;
            }
        }
        
        //- @note: Perform perspective projection
        Vec2 b0, b1;
        f32  f0, f1;
        f32  height = walls[wall_idx].height;
        f32  half_height = height/2.f;
        
        b0.x = (cam_dist/d0.y)*d0.x;
        b1.x = (cam_dist/d1.y)*d1.x;
        b0.y = (cam_dist/d0.y)*half_height;
        b1.y = (cam_dist/d1.y)*half_height;
        f0   = (cam_dist/d0.y)*-half_height;
        f1   = (cam_dist/d1.y)*-half_height;
        
        //- @note: NDC -> Screen coordinates
#define ndc_to_screen_x(x) width_middle*(x)+((canvas->width-1.f)/2.f)
#define ndc_to_screen_y(y) height_middle*(y)+((canvas->height-1.f)/2.f)
        
        Vec2 p0, p1, p2, p3;
        p0.x = ndc_to_screen_x(b0.x);
        p0.y = ndc_to_screen_y(f0);
        
        p1.x = p0.x;
        p1.y = ndc_to_screen_y(b0.y);
        
        p2.x = ndc_to_screen_x(b1.x);
        p2.y = ndc_to_screen_y(b1.y);
        
        p3.x = p2.x;
        p3.y = ndc_to_screen_y(f1);
        
#undef ndc_to_screen_x
#undef ndc_to_screen_y
        
        //- @note: Draw wall
        r_draw_quad_frame(p0, p1, p2, p3, Color_Red);
        
#if 0
        //- @note: Minimap
#define MINIMAP_SCALE 0.3f
        Vec2 fixed_player = v2(width_middle, height_middle);
        f32 player_radius = 10.f * MINIMAP_SCALE;
        f32 turn_indicator_length = 20.f * MINIMAP_SCALE;
        Vec2 player_minimap_pos = v2muls(fixed_player, MINIMAP_SCALE);
        r_draw_circle(player_minimap_pos, player_radius, Color_White);
        r_draw_line(player_minimap_pos,  v2add(player_minimap_pos, v2(cosf(forward) * turn_indicator_length, sinf(forward) * turn_indicator_length)), Color_Magenta);
        r_draw_line(v2muls(v2add(vpp[0], fixed_player), MINIMAP_SCALE), v2muls(v2add(vpp[1], fixed_player), MINIMAP_SCALE), Color_Lime);
        
        r_draw_line(v2muls(v2add(d0, fixed_player), MINIMAP_SCALE), v2muls(v2add(d1, fixed_player), MINIMAP_SCALE), Color_Red);
        
        r_draw_quad_framef(0, 0, canvas->width * MINIMAP_SCALE, canvas->height * MINIMAP_SCALE, Color_Blue);
#endif
    }
}