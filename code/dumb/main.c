#include <Windows.h>

#include "base/include.h"

#include "base/include.c"

#define debug_print(str8) OutputDebugString((const char*)str8.str)

typedef struct Win32_Data {
    HINSTANCE hInstance;
    HWND hwnd;
    HDC win_dc;
    BITMAPINFO bitmap;
} Win32_Data;

typedef struct Bitmap_Data {
    u32 *pixels;
    u32 width, height;
} Bitmap_Data;

typedef struct Entity {
    Vec2 pos;
    f32 rotation_angle;
} Entity;

global b32 g_game_running = true;
global int g_window_width = 1280;
global int g_window_height = 720;

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

function
put_pixel_at (Bitmap_Data *canvas, Vec2 p, Color c) {
    if (p.X >= 0 && p.Y >= 0 && p.X <= canvas->width && p.Y <= canvas->height)
        canvas->pixels[(int)p.Y * canvas->width + (int)p.X] = ((int)c.R << 16 | (int)c.G << 8 | (int)c.B);
}

function void
clear_screen (Bitmap_Data *bitmap) {
    memory_zero(bitmap->pixels, bitmap->width * bitmap->height * sizeof(u32));
}

// @slow
function void
draw_circle_2d (Bitmap_Data *canvas, Vec2 pos, f32 radius, Color color) {
    for (f32 y = -radius; y <= radius; ++y) {
        for (f32 x = -radius; x <= radius; ++x) {
            if (sq(x) + sq(y) <= sq(radius))
                put_pixel_at(canvas, HMM_V2(x + pos.X, y + pos.Y), color);
        }
    }
}

// @slow
function void
draw_line_low_2d (Bitmap_Data *canvas, Vec2 p0, Vec2 p1, Color color) {
    f32 dx = p1.X - p0.X;
    f32 dy = p1.Y - p0.Y;
    f32 yi = 1.f;
    if (dy < 0) {
        yi = -1.f;
        dy = -dy;
    }
    f32 d = 2.f*dy - dx;
    f32 y = p0.Y;
    
    for (f32 x = p0.X; x <= p1.X; ++x) {
        put_pixel_at(canvas, HMM_V2(x,y), color);
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
draw_line_high_2d (Bitmap_Data *canvas, Vec2 p0, Vec2 p1, Color color) {
    f32 dx = p1.X - p0.X;
    f32 dy = p1.Y - p0.Y;
    f32 xi = 1.f;
    if (dx < 0) {
        xi = -1.f;
        dx = -dx;
    }
    f32 d = 2.f*dx - dy;
    f32 x = p0.X;
    
    for (f32 y = p0.Y; y <= p1.Y; ++y) {
        put_pixel_at(canvas, HMM_V2(x,y), color);
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
draw_line_2d (Bitmap_Data *canvas, Vec2 p0, Vec2 p1, Color color) {
    if (fabs(p1.Y - p0.Y) < fabs(p1.X - p0.X)) {
        if (p0.X > p1.X)
            draw_line_low_2d(canvas, p1, p0, color);
        else
            draw_line_low_2d(canvas, p0, p1, color);
    } else {
        if (p0.Y > p1.Y)
            draw_line_high_2d(canvas, p1, p0, color);
        else
            draw_line_high_2d(canvas, p0, p1, color);
    }
}

int
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    //- @note: Platform setup
    
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
    
    // @note: Create bitmap
    Bitmap_Data bitmap = {0};
    bitmap.width = 640;
    bitmap.height = 360;
    bitmap.pixels = arena_pushn(perm_arena, u32, bitmap.width * bitmap.height);
    platform.bitmap = win32_create_bitmap(&bitmap);
    
    //- @note: Game setup
    Entity player = {0};
    player.pos.X = bitmap.width / 2.f;
    player.pos.Y = bitmap.height / 2.f;
    
    //- @note: Main loop
    for (;g_game_running;) {
        arena_clear(frame_arena);
        
        //~ @note: Message loop
        
        // @todo: It is annoying to need to pull out these "action commands"
        local_persist b32 move_up, move_down, move_left, move_right, turn_left, turn_right; 
        for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
            if (msg.message == WM_QUIT) {
                g_game_running = false;
            }
            
            // @todo: Maybe move this to WndProc?
            if (msg.message == WM_INPUT) {
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
                        
                        // @todo: Why is this finicky on startup?
                        switch (scan_code) {
                            case 0x0011: fallthrough; // W / up-arrow
                            case 0xE048: {
                                move_up = key_down;
                            } break;
                            
                            
                            case 0x001F: fallthrough; // S / down-arrow
                            case 0xE050: {
                                move_down = key_down;
                            } break;
                            
                            
                            case 0x001E: fallthrough; // A / left-arrow
                            case 0xE04B: {
                                move_left = key_down;
                            } break;
                            
                            
                            case 0x0020: fallthrough; // D / right-arrow
                            case 0xE04D: {
                                move_right = key_down;
                            } break;
                            
                            case 0x0010: { // Q
                                turn_left = key_down;
                            } break;
                            
                            case 0x0012: { // E
                                turn_right = key_down;
                            } break;
                        }
                    }
                }
            }
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        //~ @note: Update
        
        Vec2 dir = {0};
        if (move_up)    dir.Y +=  1;
        if (move_down)  dir.Y += -1;
        if (move_left)  dir.X += -1;
        if (move_right) dir.X +=  1;
        if (HMM_LenSqrV2(dir) > 1)
            dir = HMM_NormV2(dir);
        player.pos = HMM_AddV2(player.pos, HMM_MulV2F(dir, 1.f/8.f));
        
        if (turn_left)  player.rotation_angle += 0.0005f;
        if (turn_right) player.rotation_angle -= 0.0005f;
        player.rotation_angle = fmod_cycling(player.rotation_angle, 2 * HMM_PI32);
        
        
        //~ @note: Render
        
        clear_screen(&bitmap);
        
        draw_circle_2d(&bitmap, player.pos, 10, HMM_V4(255, 255, 255, 0));
        draw_line_2d(&bitmap, player.pos, HMM_AddV2(player.pos, HMM_V2(cosf(player.rotation_angle) * 20, sinf(player.rotation_angle) * 20)), HMM_V4(255, 0, 255, 0));
        
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