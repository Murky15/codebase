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
    Bitmap *canvas = r_get_framebuffer();
    f32 start_y = max(-1.f, y0);
    f32 end_y = min(y1, canvas->height);
    for (f32 y = start_y; y <= end_y; ++y)
        r_put_pixel_at(v2(x,y), c);
}

function void
r_draw_hori (f32 y, f32 x0, f32 x1, Range bounds, Color c) {
    if (x0 > x1)
        swap(f32, x0, x1);
    f32 start_x = max(bounds.first, x0);
    f32 end_x = min(x1, bounds.last);
    for (f32 x = start_x; x <= end_x; ++x)
        r_put_pixel_at(v2(x,y), c);
}

function void
r_draw_vert_textured (f32 x, f32 y0, f32 y1, f32 actual_height, PNG_Bitmap_RGBA texture, Texture_Map_Type map_type, s32 texx) {
    Color c;
    Bitmap *canvas = r_get_framebuffer();
    f32 start_y = max(-1.f, y0);
    f32 end_y = min(y1, canvas->height);
    f32 img_height = texture.height;
    f32 pages_per_wall = (actual_height * TEXTURE_VERT_REPEAT_SCALE) / img_height;
    for (f32 y = start_y; y <= end_y; ++y) {
        f32 ynorm = norm(y, y0, y1);
        s32 texy;
        if (map_type == TEXTURE_MAP_FIT) {
            texy = lerp(0, texture.height, ynorm);
        } else if (map_type == TEXTURE_MAP_REPEAT) {
            texy = lerp(0, texture.height*pages_per_wall, ynorm);
            texy %= (u32)img_height;
        }
        texy = img_height - texy - 1;
        c.packed = texture.pixels[texy * texture.width + texx];
        r_put_pixel_at(v2(x,y), c);
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

function void
r_draw_rect (Vec2 p, Vec2 sz, Color c) {
    for (f32 x = p.x; x <= p.x + sz.x; ++x)
        r_draw_vert(x, p.y, p.y + sz.y, c);
}

function Edge
r_make_edge (Vec2 p0, Vec2 p1) {
    Edge result = {0};
    
    if (p0.y < p1.y) {
        result.minp = p0;
        result.maxp = p1;
    } else {
        result.minp = p1;
        result.maxp = p0;
    }
    
    f32 slope = (p1.y-p0.y)/(p1.x-p0.x);
    result.slope = slope;
    result.recslope = 1.f/slope;
    
    return result;
}

function void
r_edge_array_insert (Edge_Array *array, Edge edge, s32 index) {
    assert(index < EDGE_ARRAY_COUNT);
    
    for (s32 i = array->count; i > index; --i)
        array->edges[i] = array->edges[i-1];
    array->edges[index] = edge;
    array->count++;
}

function void
r_edge_array_add (Edge_Array *array, Edge edge) {
    if (!almost_equal(edge.slope, 0.f)) {
        s32 index = 0;
        for (;index < array->count; ++index) {
            Edge *e = &array->edges[index];
            if (floorf(edge.minp.y) > floorf(e->minp.y))
                continue; 
            if (edge.minp.x > e->minp.x && almost_equal(floorf(edge.minp.y),floorf(e->minp.y)))
                continue;
            break;
        }
        r_edge_array_insert(array, edge, index);
    }
    
    if (edge.maxp.y > array->top)
        array->top = edge.maxp.y;
}

function void
r_draw_plane (Edge_Array *edges, Range bounds) {
    // Initialize fill algorithm
    f32 scan_line = edges->edges[0].minp.y;
    s32 start_idx = 0;
    
    Edge active_edges[2] = {0};
    for (s32 i=start_idx,j=0; (i < edges->count) && (j < array_count(active_edges)); ++i) {
        Edge e = edges->edges[i];
        if (floorf(e.minp.y) <= scan_line)
            active_edges[j++] = e;
    }
    start_idx += 2;
    
    // Fill polygon
    f32 x[array_count(active_edges)] = {active_edges[0].minp.x, active_edges[1].minp.x};
    while (ceil(scan_line) < edges->top) {
        r_draw_hori(scan_line, x[0], x[1], bounds, Color_Navy);
        scan_line++;
        x[0] = x[0] + active_edges[0].recslope;
        x[1] = x[1] + active_edges[1].recslope;
        for (s32 i = start_idx; i < edges->count; ++i) {
            Edge e = edges->edges[i];
            for (s32 j = 0; j < array_count(active_edges); ++j) {
                if (floor(active_edges[j].maxp.y) < scan_line && floor(e.minp.y) < scan_line) {
                    active_edges[j] = e;
                    x[j] = active_edges[j].minp.x;
                    start_idx++;
                    break;
                }
            }
        }
    }
}

function void
r_sector (Map *map, Sector *sector, Asset_Group environment_textures, Entity *cam, s32 last_sector, s32 num_iterations, Range window) {
    Bitmap *canvas = r_get_framebuffer();
    
    local_persist read_only f32 forward = M_PI32 / 2.f;
    local_persist read_only f32 near_plane = 0.001f;
    f32 canvas_width = (f32)canvas->width;
    f32 canvas_height = (f32)canvas->height;
    f32 width_middle = canvas->width/2.f;
    f32 height_middle = canvas->height/2.f;
    
    f32 actual_height = cam->height + (f32)sector->floor;
    f32 full_height = (f32)sector->ceiling - actual_height;
    f32 full_depth  = (f32)sector->floor - actual_height;
    
    if (num_iterations < MAX_ITERATIONS) {
        Edge_Array floor_edges = {0};
        Edge_Array ceil_edges = {0};
        
        for (u64 wall_idx = 0; wall_idx < sector->num_walls; ++wall_idx) {
            Wall *wall = &sector->walls[wall_idx];
            
            s32 ceil_diff = 0, floor_diff = 0;
            if (wall->next_sector >= 0) {
                Sector *next = &map->sectors[wall->next_sector];
                ceil_diff = next->ceiling - sector->ceiling;
                floor_diff = next->floor - sector->floor;
            }
            
            f32 ceiling = full_height + ceil_diff;
            f32 floor = full_depth + floor_diff;
            
            //- Transform wall relative to player
            
            Vec2 t0 = v2sub(wall->p0, cam->pos);
            Vec2 t1 = v2sub(wall->p1, cam->pos);
            
            f32 t = -cam->rotation_angle + forward;
            Vec2 d0, d1;
            d0.x = t0.x * cosf(t) - t0.y * sinf(t); 
            d0.y = t0.x * sinf(t) + t0.y * cosf(t);
            d1.x = t1.x * cosf(t) - t1.y * sinf(t);
            d1.y = t1.x * sinf(t) + t1.y * cosf(t);
            
            // Clip walls behind camera
            if (d0.y < near_plane && d1.y < near_plane)
                continue;
            
            Vec2 d0_preclip = d0;
            Vec2 d1_preclip = d1;
            f32 clipped_x = d0.x + (((d1.x - d0.x) * (near_plane - d0.y)) / (d1.y - d0.y));
            if (d0.y < near_plane) {
                d0 = v2(clipped_x, near_plane);
            } else if (d1.y < near_plane) {
                d1 = v2(clipped_x, near_plane);
            }
            
            //- Perspective projection
            f32 z0 = d0.y;
            f32 z1 = d1.y;
            f32 cam_dist = 0.89f * ASPECT_H; // 90 Degree horizontal FOV
            
#define proj_x(x,z) (((x*canvas_width)/(z*ASPECT_W))*cam_dist)+width_middle
#define proj_y(y,z) (((y*canvas_height)/(z*ASPECT_H))*cam_dist)+height_middle
            
            f32 x0_preclip = proj_x(d0_preclip.x, d0_preclip.y);
            f32 x0      = proj_x(d0.x,z0);
            f32 floor0  = proj_y(floor,z0);
            f32 ceil0   = proj_y(ceiling,z0);
            f32 depth0  = proj_y(full_depth,z0);
            f32 height0 = proj_y(full_height,z0);
            f32 x1_preclip = proj_x(d1_preclip.x, d1_preclip.y);
            f32 x1      = proj_x(d1.x,z1);
            f32 floor1  = proj_y(floor,z1);
            f32 ceil1   = proj_y(ceiling,z1);
            f32 depth1  = proj_y(full_depth,z1);
            f32 height1 = proj_y(full_height,z1);
            
#undef proj_x
#undef proj_y
            
            struct { f32 x,z, x_preclip,x_orig, floor,ceil, depth,height; } temp, minp, maxp;
            
            minp.x = x0;
            minp.x_preclip = x0_preclip;
            minp.x_orig = d0.x;
            minp.z = d0_preclip.y;
            minp.floor = floor0;
            minp.ceil = ceil0;  
            minp.depth = depth0;
            minp.height = height0;
            
            maxp.x = x1;
            maxp.x_preclip = x1_preclip;
            maxp.x_orig = d1.x;
            maxp.z = d1_preclip.y;
            maxp.floor = floor1;
            maxp.ceil = ceil1;
            maxp.depth = depth1;
            maxp.height = height1;
            
            if (maxp.x < minp.x) {
                temp = minp;
                minp = maxp;
                maxp = temp; 
            }
            
            f32 start_x = clamp(minp.x, window.first, window.last);
            f32 end_x = clamp(maxp.x, window.first, window.last);   
            
            // Render into next scene (if applicable)
            if (wall->next_sector >= 0 && wall->next_sector != last_sector) { // @todo: This will result in an infinite loop for "circular" sector chains
                Sector *next_sector = &map->sectors[wall->next_sector];
                Range bounds;
                bounds.first = start_x;
                bounds.last  = end_x;
                Entity modified_cam = *cam;
                modified_cam.height = actual_height - next_sector->floor;
                r_sector(map, next_sector, environment_textures, &modified_cam, sector->id, num_iterations + 1, bounds);
            }
            
            Asset test_wall_texture = asset_group_fetch(&environment_textures, str8_lit("BRICK_1A.PNG"));
            Asset test_floor_texture = asset_group_fetch(&environment_textures, str8_lit("COBBLES_1B.PNG"));
            Texture_Map_Type test_texture_map_type = TEXTURE_MAP_REPEAT;
            
            f32 img_width = test_wall_texture.img.width;
            f32 wall_length = sqrtf(sqr(d0_preclip.x-d1_preclip.x) + sqr(d0_preclip.y-d1_preclip.y)); // @todo: Cache this when loading level json
            f32 pages_per_wall = wall_length / img_width;
            
            //- Render walls
            for (f32 x = start_x; x <= end_x; ++x) {
                f32 xnorm  = norm(x, minp.x, maxp.x);
                f32 texnorm = norm(x, minp.x_preclip, maxp.x_preclip);
                
                // Wall Texture mapping
                s32 texx;
                if (test_texture_map_type == TEXTURE_MAP_FIT) {                                          
                    texx = lerp(0, img_width/maxp.z, texnorm) / lerp(1.f/minp.z, 1.f/maxp.z, texnorm);
                } else if (test_texture_map_type == TEXTURE_MAP_REPEAT) {
                    texx = lerp(0, (img_width*pages_per_wall)/maxp.z, texnorm) / lerp(1.f/minp.z, 1.f/maxp.z, texnorm);
                    texx %= (u32)img_width;
                }
                
                f32 depth  = lerp(minp.depth, maxp.depth, xnorm); 
                f32 height = lerp(minp.height, maxp.height, xnorm);
                f32 floor  = lerp(minp.floor, maxp.floor, xnorm); 
                f32 ceil   = lerp(minp.ceil, maxp.ceil, xnorm);
                
                //r_draw_vert(x, depth, floor, Color_Maroon); // ledge
                if (wall->next_sector == -1) r_draw_vert_textured(x, floor, ceil, sector->ceiling-sector->floor, test_wall_texture.img, test_texture_map_type, texx); // wall
                //r_draw_vert(x, ceil, height, Color_Maroon); // ledge
                
                r_draw_vert(x, -1.f, depth, Color_Black); // floor
                r_draw_vert(x, height, canvas_height, Color_Black); // Cielling
            } 
            
            // Add floor / ceiling edges
            Vec2 f0 = v2(minp.x, minp.depth), f1 = v2(maxp.x, maxp.depth);
            if (!(f0.y < 0.f && f1.y < 0.f)) {
                f32 depth_norm = norm(-1.f, minp.depth, maxp.depth);
                f32 x = lerp(minp.x, maxp.x, depth_norm);
                if (f0.y < 0.f)
                    f0 = v2(x, -1.f);
                else if (f1.y < 0.f) 
                    f1 = v2(x, -1.f);
                Edge edge = r_make_edge(f0, f1);
                r_edge_array_add(&floor_edges, edge);
            }
            
            Vec2 c0 = v2(minp.x, minp.height), c1 = v2(maxp.x, maxp.height);
            if (!(c0.y > canvas_height && c1.y > canvas_height)) {
                f32 height_norm = norm(canvas_height, minp.height, maxp.height);
                f32 x = lerp(minp.x, maxp.x, height_norm);
                if (c0.y > canvas_height) 
                    c0 = v2(x, canvas_height);
                else if (c1.y > canvas_height) 
                    c1 = v2(x, canvas_height);
                Edge edge = r_make_edge(c0, c1);
                r_edge_array_add(&ceil_edges, edge);
            }
        }
        //- Render floor and ceiling
        r_draw_plane(&floor_edges, window);
        r_draw_plane(&ceil_edges, window);
    }
}