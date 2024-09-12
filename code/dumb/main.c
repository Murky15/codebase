//~ @note: Unity build
//- @note: Headers
#include <Windows.h>
#include "base/include.h"
//- @note: Source
#include "base/include.c"

/*
@todo
- [ ] sin/cos/tan table lookup: https://namoseley.wordpress.com/2015/07/26/sincos-generation-using-table-lookup-and-iterpolation/
*/

#define MINIMAP_SCALE 0.3f
#define PLAYER_MOVE_SPEED 100.f

typedef struct Win32_Data {
    HINSTANCE hInstance;
    HWND hwnd;
    HDC win_dc;
    BITMAPINFO bitmap;
} Win32_Data;

typedef struct Entity {
    Vec2 pos;
    f32 rotation_angle;
    f32 radius;
} Entity;

typedef struct Wall {
    Vec2 p0, p1;
    f32 height;
} Wall;

global b32 g_game_running = true;
global int g_window_width = 1280;
global int g_window_height = 720;
global b32 g_draw_minimap = true;

function LRESULT
Wndproc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE: PostQuitMessage(0); return 0; break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

function Win32_Data
win32_create_window (HINSTANCE hInstance) {
    WNDCLASS wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = Wndproc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = "dumb_main_window_class";
    RegisterClass(&wc);
    
    RECT dim = {0, 0, g_window_width, g_window_height};
    AdjustWindowRect(&dim, WS_OVERLAPPEDWINDOW, 0);
    HWND hwnd = CreateWindow(
                             wc.lpszClassName,
                             "Dumb: DEV BUILD",
                             (WS_OVERLAPPEDWINDOW | WS_VISIBLE) ^ WS_THICKFRAME, // @todo: Handle window resizes
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             dim.right - dim.left,
                             dim.bottom - dim.top,
                             0, 0,
                             hInstance,
                             0
                             );
    if (!hwnd) {
        OutputDebugString("Window creation failed!\n");
        return (Win32_Data){0};
    }
    
    Win32_Data result = {0};
    result.hInstance = hInstance;
    result.hwnd = hwnd;
    result.win_dc = GetDC(hwnd);
    
    return result;
}

function BITMAPINFO
win32_create_bitmap (Bitmap_Data *data) {
    BITMAPINFOHEADER header = {0};
    header.biSize = sizeof(header);
    header.biWidth = data->width;
    header.biHeight = data->height; // @note: negate for top-down
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    return (BITMAPINFO){header};
}

function void
render_silly_little_thing (u32 *pixels, u32 width, u32 height) {
    local_persist u8 xOffset, yOffset;
    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            u8 r = (u8)x + xOffset;
            u8 g = (u8)y + yOffset;
            u8 b = 0;
            pixels[y * width + x] = (r << 16 | g << 8 | b);
        }
    }
    xOffset = ++yOffset;
}

function void
put_pixel_at (Bitmap_Data *canvas, Vec2 p, Color col) {
    Vec2i c = v2i_from_v2(p);
    if (c.x >= 0 && c.y >= 0 && c.x <= canvas->width && c.y <= canvas->height)
        canvas->pixels[c.y * canvas->width + c.x] = (col.r << 16 | col.g << 8 | col.b);
}

// @todo: Handle more colors than just black?
function void
clear_screen (Bitmap_Data *bitmap) {
    memory_zero(bitmap->pixels, bitmap->width * bitmap->height * sizeof(u32));
}

// @slow
function void
draw_circle (Bitmap_Data *canvas, Vec2 pos, f32 radius, Color color) {
    for (f32 y = -radius; y <= radius; ++y) {
        for (f32 x = -radius; x <= radius; ++x) {
            if (sq(x) + sq(y) <= sq(radius))
                put_pixel_at(canvas, v2(x + pos.x, y + pos.y), color);
        }
    }
}

// @slow
function void
draw_line_low (Bitmap_Data *canvas, Vec2 p0, Vec2 p1, Color color) {
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
        put_pixel_at(canvas, v2(x,y), color);
        if (d > 0) {
            y += yi;
            d += (2.f * (dy - dx));
        } else {
            d += 2.f * dy;
        }
    }
}

// @slow
function void
draw_line_high (Bitmap_Data *canvas, Vec2 p0, Vec2 p1, Color color) {
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
        put_pixel_at(canvas, v2(x,y), color);
        if (d > 0) {
            x += xi;
            d += (2.f * (dx - dy));
        } else {
            d += 2.f * dx;
        }
    }
}

// @slow
function void
draw_line (Bitmap_Data *canvas, Vec2 p0, Vec2 p1, Color color) {
    if (fabs(p1.y - p0.y) < fabs(p1.x - p0.x)) {
        if (p0.x > p1.x)
            draw_line_low(canvas, p1, p0, color);
        else
            draw_line_low(canvas, p0, p1, color);
    } else {
        if (p0.y > p1.y)
            draw_line_high(canvas, p1, p0, color);
        else
            draw_line_high(canvas, p0, p1, color);
    }
}

// @slow: This may be the worst rasterizer anyone has ever created
function void
draw_quad_framef (Bitmap_Data *canvas, f32 x0, f32 y0, f32 x1, f32 y1, Color color) {
    draw_line(canvas, v2(x0, y0), v2(x1, y0), color);
    draw_line(canvas, v2(x1, y0), v2(x1, y1), color);
    draw_line(canvas, v2(x1, y1), v2(x0, y1), color);
    draw_line(canvas, v2(x0, y1), v2(x0, y0), color);
}

function void
draw_quad_frame (Bitmap_Data *canvas, Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, Color color) {
    draw_line(canvas, p0, p1, color);
    draw_line(canvas, p1, p2, color);
    draw_line(canvas, p2, p3, color);
    draw_line(canvas, p3, p0, color);
}

int
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    //~ @note: Platform setup
    
    Arena *perm_arena = arena_alloc();
    Arena *frame_arena = arena_alloc();
    
    Win32_Data platform = win32_create_window(hInstance);
    
    // @note: Register for input
    RAWINPUTDEVICE input_devices[1];
    
    // Keyboard
    input_devices[0].usUsagePage = 0x01;
    input_devices[0].usUsage = 0x06;
    input_devices[0].dwFlags = RIDEV_NOLEGACY;
    input_devices[0].hwndTarget = 0;
    
#if 0
    // Mouse
    input_devices[1].usUsagePage = 0x01;
    input_devices[1].usUsage = 0x02;
    input_devices[1].dwFlags = 0; // For some reason RIDEV_NOLEGACY hangs program?
    input_devices[1].hwndTarget = 0;
#endif
    
    if (RegisterRawInputDevices(input_devices, array_count(input_devices), sizeof(input_devices[0])) == FALSE) {
        OutputDebugString("Unable to register input devices\n");
    }
    
    // @note: Timing
    LARGE_INTEGER frequency, start_time, end_time, elapsed_microseconds = {0};
    QueryPerformanceFrequency(&frequency);
    
    // @note: Create bitmap
    Bitmap_Data bitmap = {0};
    bitmap.width = 640;
    bitmap.height = 360;
    bitmap.pixels = malloc(bitmap.width * bitmap.height * sizeof(u32));
    platform.bitmap = win32_create_bitmap(&bitmap);
    
    //~ @note: Game setup
    // @todo: Constant perspective projection matrix here?
    
    Entity player = {0};
    player.rotation_angle = 0;
    player.radius = 10.f;
    player.pos.x = bitmap.width / 2.f;
    player.pos.y = bitmap.height / 2.f;
    
    Wall test_wall = {0};
    test_wall.p0 = v2(500, 100);
    test_wall.p1 = v2(500, 200);
    test_wall.height = 50.f;
    
    //~ @note: Main loop
    QueryPerformanceCounter(&start_time);
    for (;g_game_running;) {
        arena_clear(frame_arena);
        
        //~ @note: Message loop
        
        // @todo: It is annoying to need to pull out these "action commands"
        local_persist b32 move_forward, move_back, strafe_left, strafe_right, turn_left, turn_right; 
        for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
            if (msg.message == WM_QUIT) {
                g_game_running = false;
            } else if (msg.message == WM_INPUT) {
                // @todo: Maybe move this to WndProc?
                UINT size;
                local_persist u8 data[sizeof(RAWINPUT)];
                GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER));
                RAWINPUT *input = (RAWINPUT*)data;
                
                if (input->header.dwType == RIM_TYPEKEYBOARD) {
                    RAWKEYBOARD *keyboard = &input->data.keyboard;
                    b32 key_down = (keyboard->Flags & RI_KEY_BREAK) == 0;
                    if (keyboard->MakeCode != KEYBOARD_OVERRUN_MAKE_CODE) {
                        u8 extension = keyboard->Flags & RI_KEY_E0 ? 0xE0 : keyboard->Flags & RI_KEY_E1 ? 0xE1 : 0x00;
                        u16 scan_code = (extension << 8) | (keyboard->MakeCode & 0x7F);
                        
                        switch (scan_code) {
                            case 0x0011: fallthrough; // W / up-arrow
                            case 0xE048: {
                                move_forward = key_down;
                            } break;
                            
                            
                            case 0x001F: fallthrough; // S / down-arrow
                            case 0xE050: {
                                move_back = key_down;
                            } break;
                            
                            
                            case 0x001E: fallthrough; // A / left-arrow
                            case 0xE04B: {
                                strafe_left = key_down;
                            } break;
                            
                            
                            case 0x0020: fallthrough; // D / right-arrow
                            case 0xE04D: {
                                strafe_right = key_down;
                            } break;
                            
                            case 0x0010: {            // Q
                                turn_left = key_down;
                            } break;
                            
                            case 0x0012: {            // E
                                turn_right = key_down;
                            } break;
                        }
                    }
                }
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        
        //- @note: Update
        QueryPerformanceCounter(&end_time);
        elapsed_microseconds.QuadPart = end_time.QuadPart - start_time.QuadPart;
        f32 dt = (f32)((f32)elapsed_microseconds.QuadPart / (f32)frequency.QuadPart);
        start_time = end_time;
        
        Vec2 dir;
        f32 x = 0, y = 0;
        if (move_forward)   x +=  1;
        if (move_back)      x += -1;
        if (strafe_left)    y +=  1;
        if (strafe_right)   y += -1;
        dir.x = x * cosf(player.rotation_angle) - y * sinf(player.rotation_angle);
        dir.y = x * sinf(player.rotation_angle) + y * cosf(player.rotation_angle);
        if (v2len(dir) > 1)
            dir = v2norm(dir);
        
        player.pos = v2add(player.pos, v2muls(dir, PLAYER_MOVE_SPEED * dt));
        player.pos.x = clamp(player.pos.x, player.radius, bitmap.width - player.radius);
        player.pos.y = clamp(player.pos.y, player.radius, bitmap.height - player.radius);
        
        f32 turn_amount = 1.5f;
        if (turn_left)  player.rotation_angle += turn_amount * dt;
        if (turn_right) player.rotation_angle -= turn_amount * dt;
        player.rotation_angle = fmod_cycling(player.rotation_angle, 2 * M_PI32);
        
        //- @note: Render
        
        clear_screen(&bitmap);
        
        // @note: 3D shit (I have no clue what I am doing)
        // @very-very-slow
        
        // @note: Transform world relative to player
        Vec2 fixed_player = v2(bitmap.width / 2.f, bitmap.height / 2.f);
        Vec2 cam_pos = player.pos;
        f32 forward = M_PI32 / 2.f;
        f32 theta = -player.rotation_angle + forward;
        Vec2 t0 = v2sub(test_wall.p0, cam_pos);
        Vec2 t1 = v2sub(test_wall.p1, cam_pos);
        
        Vec2 d0, d1;
        d0.x = t0.x * cosf(theta) - t0.y * sinf(theta);
        d0.y = t0.x * sinf(theta) + t0.y * cosf(theta);
        d1.x = t1.x * cosf(theta) - t1.y * sinf(theta);
        d1.y = t1.x * sinf(theta) + t1.y * cosf(theta);
        
        assert(d0.x != 0 && d1.x != 0);
        
        // @note: Clip walls behind player 
        // https://www.jonolick.com/home/unusual-cross-product-tricks#:~:text=Then%2C%20you%20can%20find%20the,you%20have%20three%203D%20points.
        Vec3 vpp[2] = {{-fixed_player.x, 1, 1}, {fixed_player.x, 1, 1}};
        
        f32 d0_visibility = v2cross(dv3(vpp[0]), dv3(vpp[1]), d0);
        f32 d1_visibility = v2cross(dv3(vpp[0]), dv3(vpp[1]), d1);
        if (d0_visibility >= 0 || d1_visibility >= 0) {
            Vec3 view_plane = v3norm(v3cross(vpp[0], vpp[1]));
            Vec3 wall_line = v3norm(v3cross(pv2(d0, 1), pv2(d1, 1)));
            Vec3 intersection = v3cross(view_plane, wall_line);
            if (intersection.z != 0) {
                Vec2 in = v2(intersection.x / intersection.z, intersection.y / intersection.z);
                f32 minx = min(d0.x, d1.x);
                f32 maxx = max(d0.x, d1.x);
                f32 miny = min(d0.y, d1.y);
                f32 maxy = max(d0.y, d1.y);
                if ((minx < in.x && in.x < maxx && miny < in.y && in.y < maxy) && in.x != 0 && in.y != 0) {
                    if (d0_visibility <= 0)
                        d0 = in;
                    else if (d1_visibility <= 0)
                        d1 = in;
                }
            }
            
            Vec2 b0, b1;
            f32 cam_dist = 1.f;
            f32 screen_middle = ((f32)bitmap.height / 2.f); 
            
            b0.x = (cam_dist/d0.y) * d0.x;
            b0.y = (cam_dist/d0.y) * (test_wall.height/2.f);
            b1.x = (cam_dist/d1.y) * d1.x;
            b1.y = (cam_dist/d1.y) * (test_wall.height/2.f);
            
            f32 p_base0 = (cam_dist/d0.y) * (-test_wall.height/2.f);
            f32 p_base1 = (cam_dist/d1.y) * (-test_wall.height/2.f);
            
            // Convert to screen coordinates
            // #define ndc_to_screen_x/y
            
            f32 x0 = (bitmap.width  / 2.f) * b0.x + ((bitmap.width  - 1.f) / 2.f);
            f32 y0 = (bitmap.height / 2.f) * b0.y + ((bitmap.height - 1.f) / 2.f);
            f32 x1 = (bitmap.width  / 2.f) * b1.x + ((bitmap.width  - 1.f) / 2.f);
            f32 y1 = (bitmap.height / 2.f) * b1.y + ((bitmap.height - 1.f) / 2.f);
            
            f32 base0 = (bitmap.height / 2.f) * p_base0 + ((bitmap.height - 1.f) / 2.f);
            f32 base1 = (bitmap.height / 2.f) * p_base1 + ((bitmap.height - 1.f) / 2.f);
            
            Vec2 p0, p1, p2, p3;
            p0.x = x0;
            p0.y = base0;
            
            p1.x = x0;
            p1.y = y0;
            
            p2.x = x1;
            p2.y = y1;
            
            p3.x = x1;
            p3.y = base1;
            
            draw_quad_frame(&bitmap, p0, p1, p2, p3, Color_Red);
            
        }
        // @note: Minimap
        if (g_draw_minimap)
        {
            f32 player_radius = player.radius * MINIMAP_SCALE;
            f32 turn_indicator_length = 20.f * MINIMAP_SCALE;
            Vec2 player_minimap_pos = v2muls(fixed_player, MINIMAP_SCALE);
            draw_circle(&bitmap, player_minimap_pos, player_radius, Color_White);
            draw_line(&bitmap, player_minimap_pos,  v2add(player_minimap_pos, v2(cosf(forward) * turn_indicator_length, sinf(forward) * turn_indicator_length)), Color_Magenta);
            draw_line(&bitmap, v2muls(v2add(dv3(vpp[0]), fixed_player), MINIMAP_SCALE), v2muls(v2add(dv3(vpp[1]), fixed_player), MINIMAP_SCALE), Color_Lime);
            
            //draw_circle(&bitmap, v2muls(v2add(in, fixed_player), MINIMAP_SCALE), 2.f, Color_Yellow);
            draw_line(&bitmap, v2muls(v2add(d0, fixed_player), MINIMAP_SCALE), v2muls(v2add(d1, fixed_player), MINIMAP_SCALE), Color_Red);
            
            draw_quad_framef(&bitmap, 0, 0, bitmap.width * MINIMAP_SCALE, bitmap.height * MINIMAP_SCALE, Color_Blue);
        }
        
        StretchDIBits(
                      platform.win_dc,
                      0, 0,
                      g_window_width,
                      g_window_height,
                      0, 0,
                      bitmap.width,
                      bitmap.height,
                      bitmap.pixels,
                      &platform.bitmap,
                      DIB_RGB_COLORS,
                      SRCCOPY
                      );
    }
    
    return 0;
}