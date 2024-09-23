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
            if (sqr(x) + sqr(y) <= sqr(r))
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
    assert(y0 <= y1);
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
    f32 width_middle = canvas->width/2.f;
    f32 height_middle = canvas->height/2.f;
    f32 near_plane = 1.f;
    f32 cam_dist = 1.f;
    f32 view_angle = 2.f * atanf(1.f/cam_dist);
    
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
        
        //- @note: Clip walls behind camera
        // @todo: There's gotta be a way to make this faster and with less code
        
        // Find view boundaries of camera 
        f32 hvpx = cam_dist * tanf(view_angle/2.f);
        Vec3 wall_line = v3norm(v3cross(pv2(d0,1), pv2(d1,1)));
        
        f32 minx = min(d0.x, d1.x);
        f32 miny = min(d0.y, d1.y);
        f32 maxx = max(d0.x, d1.x);
        f32 maxy = max(d0.y, d1.y);
        
        // Check for intersections 
        Vec2 in;
        
        // Left side
        Vec2 hvp_left  = v2(-hvpx, cam_dist);
        {
            Vec3 left_boundary = v3norm(v3cross(v3(0,0,1), pv2(hvp_left,1)));
            Vec3 left_intersection = v3cross(wall_line, left_boundary);
            
            // Test visibility
            f32 v0 = v2cross(v2(0,0),hvp_left, d0);
            f32 v1 = v2cross(v2(0,0),hvp_left, d1);
            if (v0 >= 0 && v1 >= 0) 
                continue;
            
            // Check if we have an intersection and the point is on the wall line segment
            if (left_intersection.z != 0) {
                in = v2(left_intersection.x / left_intersection.z, left_intersection.y / left_intersection.z);
                if (minx <= in.x && in.x <= maxx && miny <= in.y && in.y <= maxy) {
                    if (v0 >= 0)
                        d0 = in;
                    else if (v1 >= 0)
                        d1 = in;
                }
            }
        }
        
        // Repeat on right side
        Vec2 hvp_right = v2(hvpx, cam_dist);
        {
            Vec3 right_boundary = v3norm(v3cross(v3(0,0,1), pv2(hvp_right,1)));
            Vec3 right_intersection = v3norm(v3cross(wall_line, right_boundary));
            f32 v0 = v2cross(v2(0,0),hvp_right, d0);
            f32 v1 = v2cross(v2(0,0),hvp_right, d1);
            if (v0 <= 0 && v1 <= 0) 
                continue;
            if (right_intersection.z != 0) {
                in = v2(right_intersection.x / right_intersection.z, right_intersection.y / right_intersection.z);
                if (minx <= in.x && in.x <= maxx && miny <= in.y && in.y <= maxy) {
                    if (v0 <= 0)
                        d0 = in;
                    else if (v1 <= 0)
                        d1 = in;
                }
            }
        }
        
        // Clip all walls completely behind player
        if (d0.y <= near_plane && d1.y <= near_plane) 
            continue;
        
        //- @note: Perspective projection
        
        f32 height = 50.f; // Height should be determined by sector
        f32 half_height = height/2.f;
        
        f32 z0 = d0.y;
        f32 z1 = d1.y;
        
        f32 x0    = (cam_dist/z0) *  d0.x;
        f32 ybot0 = (cam_dist/z0) * -half_height;
        f32 ytop0 = (cam_dist/z0) *  half_height;
        f32 x1    = (cam_dist/z1) *  d1.x;
        f32 ybot1 = (cam_dist/z1) * -half_height;
        f32 ytop1 = (cam_dist/z1) *  half_height;
        
        //- @note: NDC -> Screen coordinates
#define ndc_to_screen_x(x) (width_middle *x + (canvas->width -1.f)/2.f)
#define ndc_to_screen_y(y) (height_middle*y + (canvas->height-1.f)/2.f)
        struct { f32 x,ybot,ytop; } temp, minp, maxp = {0};
        minp.x    = ndc_to_screen_x(x0);
        minp.ybot = ndc_to_screen_y(ybot0);
        minp.ytop = ndc_to_screen_y(ytop0);
        maxp.x    = ndc_to_screen_x(x1);
        maxp.ybot = ndc_to_screen_y(ybot1);
        maxp.ytop = ndc_to_screen_y(ytop1);
#undef ndc_to_screen_x
#undef ndc_to_screen_y
        
        if (x0 > x1) {
            temp = minp;
            minp = maxp;
            maxp = temp;
        }
        
        for (f32 x = minp.x; x <= maxp.x; ++x) {
            f32 xnorm = norm(x, minp.x, maxp.x);
            f32 ybot = clamp(lerp(minp.ybot, maxp.ybot, xnorm), 0, height_middle);
            f32 ytop = clamp(lerp(minp.ytop, maxp.ytop, xnorm), height_middle, canvas->height);
            r_draw_vert(x, 0.f, ybot, Color_Gray); // floor
            r_draw_vert(x, ybot, ytop, walls[wall_idx].color); // wall
            r_draw_vert(x, ytop, (f32)canvas->height, Color_Cyan); // Cielling
        }
        
        
#if 0
        //- @note: Minimap
#define MINIMAP_SCALE 0.3f
        Vec2 fixed_player = v2(width_middle, height_middle);
        f32 player_radius = 10.f * MINIMAP_SCALE;
        f32 turn_indicator_length = 20.f * MINIMAP_SCALE;
        Vec2 player_minimap_pos = v2muls(fixed_player, MINIMAP_SCALE);
        // Player (camera)
        r_draw_circle(player_minimap_pos, player_radius, Color_White);
        r_draw_line(player_minimap_pos,  v2add(player_minimap_pos, v2(cosf(forward) * turn_indicator_length, sinf(forward) * turn_indicator_length)), Color_Magenta);
        
        // View boundaries
        r_draw_line(v2muls(v2add(v2(-width_middle, 0), fixed_player), MINIMAP_SCALE), v2muls(v2add(v2(width_middle, 0), fixed_player), MINIMAP_SCALE), Color_Lime);
        r_draw_line(player_minimap_pos, v2add(player_minimap_pos, v2muls(hvp_left, 50)), Color_Lime);
        r_draw_line(player_minimap_pos, v2add(player_minimap_pos, v2muls(hvp_right, 50)), Color_Lime);
        
        // Wall
        r_draw_line(v2muls(v2add(d0, fixed_player), MINIMAP_SCALE), v2muls(v2add(d1, fixed_player), MINIMAP_SCALE), Color_Red);
        
        // Border
        r_draw_quad_framef(0, 0, canvas->width * MINIMAP_SCALE, canvas->height * MINIMAP_SCALE, Color_Blue);
#endif
    }
}