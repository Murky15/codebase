function Bitmap*
r_get_framebuffer (void) {
    local_persist Bitmap f;
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

function void
r_draw_rect (Vec2 p, Vec2 sz, Color c) {
    for (f32 x = p.x; x <= p.x + sz.x; ++x)
        r_draw_vert(x, p.y, p.y + sz.y, c);
}

#define MAX_ITERATIONS 64
function void
r_sector (Map *map, Sector *sector, Entity *cam, s32 last_sector, Range window) {
    Bitmap *canvas = r_get_framebuffer();
    
    local_persist u64 num_iterations = 0;
    local_persist read_only f32 forward = M_PI32 / 2.f;
    local_persist read_only f32 near_plane = 0.001f;
    f32 canvas_width = (f32)canvas->width;
    f32 canvas_height = (f32)canvas->height;
    f32 width_middle = canvas->width/2.f;
    f32 height_middle = canvas->height/2.f;
    
    if (last_sector == -1)
        num_iterations = 0;
    else
        num_iterations++;
    
    if (num_iterations < MAX_ITERATIONS) {
        for (u64 wall_idx = 0; wall_idx < sector->num_walls; ++wall_idx) {
            //- Transform wall relative to player
            Wall *wall = &sector->walls[wall_idx];
            if (wall->next_sector >= 0 && wall->next_sector == last_sector)
                continue;
            
            Vec2 t0 = v2sub(wall->p0, cam->pos);
            Vec2 t1 = v2sub(wall->p1, cam->pos);
            
            f32 t = -cam->rotation_angle + forward;
            Vec2 d0, d1;
            d0.x = t0.x * cosf(t) - t0.y * sinf(t);
            d0.y = t0.x * sinf(t) + t0.y * cosf(t);
            d1.x = t1.x * cosf(t) - t1.y * sinf(t);
            d1.y = t1.x * sinf(t) + t1.y * cosf(t);
            
            //- Clip walls behind camera
            if (d0.y < near_plane && d1.y < near_plane)
                continue;
            
            f32 clipped_x = d0.x + (((d1.x - d0.x) * (near_plane - d0.y)) / (d1.y - d0.y));
            if (d0.y <= near_plane)
                d0 = v2(clipped_x, near_plane);
            else if (d1.y <= near_plane)
                d1 = v2(clipped_x, near_plane);
            
            //- Perspective projection
            f32 z0 = d0.y;
            f32 z1 = d1.y;
            f32 cam_dist = 0.89f * ASPECT_H; // 90 Degree horizontal FOV
            
            s32 ceil_diff = 0, floor_diff = 0;
            if (wall->next_sector >= 0) {
                Sector *next = &map->sectors[wall->next_sector];
                ceil_diff = next->ceiling - sector->ceiling;
                floor_diff = next->floor - sector->floor;
            }
            
            f32 actual_height = cam->height + (f32)sector->floor;
            f32 full_height = (f32)sector->ceiling - actual_height;
            f32 full_depth  = (f32)sector->floor - actual_height;
            f32 ceiling = full_height + ceil_diff;
            f32 floor = full_depth + floor_diff;
            
#define proj_x(x,z) (((x*canvas_width)/(z*ASPECT_W))*cam_dist)+width_middle
#define proj_y(y,z) (((y*canvas_height)/(z*ASPECT_H))*cam_dist)+height_middle
            
            f32 x0      = proj_x(d0.x,z0);
            f32 floor0  = proj_y(floor,z0);
            f32 ceil0   = proj_y(ceiling,z0);
            f32 depth0  = proj_y(full_depth,z0);
            f32 height0 = proj_y(full_height,z0);
            f32 x1      = proj_x(d1.x,z1);
            f32 floor1  = proj_y(floor,z1);
            f32 ceil1   = proj_y(ceiling,z1);
            f32 depth1  = proj_y(full_depth,z1);
            f32 height1 = proj_y(full_height,z1);
            
#undef proj_x
#undef proj_y
            
            struct { f32 x,floor,ceil,depth,height; } temp, minp, maxp;
            minp.x = x0;
            minp.floor = floor0;
            minp.ceil = ceil0;  
            minp.depth = depth0;
            minp.height = height0;
            maxp.x = x1;
            maxp.floor = floor1;
            maxp.ceil = ceil1;
            maxp.depth = depth1;
            maxp.height = height1;
            
            if (x0 > x1) {
                temp = minp;
                minp = maxp;
                maxp = temp; 
            }
            
            // Render into next scene (if applicable)
            if (wall->next_sector >= 0) {
                Sector *next_sector = &map->sectors[wall->next_sector];
                Range bounds;
                bounds.first = max(minp.x, window.first);
                bounds.last  = min(maxp.x, window.last);
                Entity modified_cam = *cam;
                modified_cam.height = actual_height - next_sector->floor;
                r_sector(map, next_sector, &modified_cam, sector->id, bounds);
            }
            
            f32 start_x = max(minp.x, -1.f);
            f32 end_x = min(maxp.x, canvas_width);
            for (f32 x = start_x; x <= end_x; ++x) {
                if (x >= window.first && x <= window.last) {
                    f32 xnorm  = norm(x, minp.x, maxp.x);
                    f32 depth  = max(lerp(minp.depth, maxp.depth, xnorm), -1);
                    f32 height = min(lerp(minp.height, maxp.height, xnorm), canvas_height);
                    f32 floor  = clamp(lerp(minp.floor, maxp.floor, xnorm), depth-1, height);
                    f32 ceil   = clamp(lerp(minp.ceil, maxp.ceil, xnorm), depth, height);
                    
                    Color wall_color = (x == start_x || x == end_x) ? Color_Black : Color_Maroon; 
                    r_draw_vert(x, -1.f, depth, Color_Blue); // floor
                    r_draw_vert(x, depth, floor, Color_Maroon); // ledge
                    if (wall->next_sector == -1) r_draw_vert(x, floor, ceil, wall_color); // wall
                    r_draw_vert(x, ceil, height, Color_Maroon); // ledge
                    r_draw_vert(x, height, canvas_height, Color_Gray); // Cielling
                }
            }
        }
    }
}

function void
r_map (Map map, Vec3 map_cam, Entity player, b32 show_player) {
    Bitmap *canvas = r_get_framebuffer();
    f32 width_middle = (f32)canvas->width/2.f;
    f32 height_middle = (f32)canvas->height/2.f;
    
    for (u64 si = 0; si < map.num_sectors; ++si) {
        Sector *sector = map.sectors + si;
        for (u64 wi = 0; wi < sector->num_walls; ++wi) {
            Wall *wall = sector->walls + wi;
            Color color = wall->next_sector >= 0 ? Color_Lime : Color_Red;
            
            Vec2 p0 = v2sub(wall->p0, dv3(map_cam));
            Vec2 p1 = v2sub(wall->p1, dv3(map_cam));
            p0.x = (p0.x*(f32)canvas->width) /(map_cam.z*ASPECT_W) + width_middle;
            p0.y = (p0.y*(f32)canvas->height)/(map_cam.z*ASPECT_H) + height_middle;
            p1.x = (p1.x*(f32)canvas->width) /(map_cam.z*ASPECT_W) + width_middle;
            p1.y = (p1.y*(f32)canvas->height)/(map_cam.z*ASPECT_H) + height_middle;
            r_draw_line(p0, p1, color);
        }
    }
    
    if (show_player) {
        Vec2 pp = v2sub(player.pos, dv3(map_cam));
        pp.x = (pp.x*(f32)canvas->width) /(map_cam.z*ASPECT_W) + width_middle;
        pp.y = (pp.y*(f32)canvas->height)/(map_cam.z*ASPECT_H) + height_middle;
        f32 radius = (player.radius / map_cam.z) * 15;
        r_draw_circle(pp, radius, Color_Magenta);
        r_draw_line(pp, v2add(pp, v2(cosf(player.rotation_angle) * 7.f, sinf(player.rotation_angle) * 7.f)), Color_Lime);
    }
}


//- @junk

#if 0

// Example minimap renderer
function void
r_draw_minimap (void) {
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
}

#endif