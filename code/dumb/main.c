//- @note: Unity build

//- @note: Headers
//#include "third_party/microui/microui.h"

#include <Windows.h>
#include <stdlib.h>
#include <time.h>

#include "base/include.h"
#include "os/include.h"
#include "json/json.h"

#include "game.h"
#include "map.h"
#include "renderer.h"

//- @note: Source
#undef RELATIVE
#undef ABSOLUTE
//#include "third_party/microui/microui.c"

#include "base/include.c"
#include "os/include.c"
#include "json/json.c"

#include "map.c"
#include "renderer.c"

// Voodoo is a sick name

/*
@todo
-[ ] Make another window using win ui for like dev tweaking n stuff
-[ ] Hot Reloading
-[ ] Read AMD programming manual
-[ ] Figure out how to do sectors and portal rendering duke nukem style (fuck me)
-[ ] Font rasterization
-[ ] Multithreading??
-[ ] SIMD???? -> compile renderer code into ISPC
-[ ] Wall texture mapping
-[ ] Optimize / profile render functions
-[ ] Asan / Libfuzzer
-[ ] sin/cos/tan table lookup: https://namoseley.wordpress.com/2015/07/26/sincos-generation-using-table-lookup-and-iterpolation/
-[ ] Make Arenas more flexible
*/

#define RESOLUTION_W 320
#define RESOLUTION_H 180

#define MOUSE_SENSITIVITY 0.005f
#define MOUSE_SCROLL_SENSITIVITY 0.8f
#define PLAYER_MOVE_SPEED 150.f
#define CAM_MOVE_SPEED 200.f

typedef struct Win32_Data {
    HINSTANCE hInstance;
    HWND hwnd;
    HDC win_dc;
    BITMAPINFO bitmap;
} Win32_Data;

global int g_window_width = 1280;
global int g_window_height = 720;
global b32 g_game_running = true;
global b32 g_mouse_captured = false;

global Vec3 map_cam;

global Arena *perm_arena;
global Arena *frame_arena;
global Arena *level_arena;

// @todo: It is annoying to need to pull out these "action commands"
global b32 move_forward, move_back, strafe_left, strafe_right;
global b32 cam_up, cam_down, cam_left, cam_right;
global f32 turn_amount;

function void
win32_capture_mouse (HWND hwnd) {
    g_mouse_captured = true;
    RECT cr;
    GetClientRect(hwnd, &cr);
    POINT middle  = {cr.right/2, cr.bottom/2};
    ClientToScreen(hwnd, &middle);
    SetCursorPos(middle.x, middle.y);
}

function LRESULT
Wndproc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE: PostQuitMessage(0); return 0;
        
        case WM_INPUT: {
            u32 size;
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
            LPBYTE buff = arena_pushn(frame_arena, BYTE, size);
            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buff, &size, sizeof(RAWINPUTHEADER)) != size)
                OutputDebugString("GetRawInputData does not return correct size !\n");
            
            RAWINPUT *input = (RAWINPUT*)buff;
            
            if (input->header.dwType == RIM_TYPEKEYBOARD) {
                RAWKEYBOARD *keyboard = &input->data.keyboard;
                b32 key_down = (keyboard->Flags & RI_KEY_BREAK) == 0;
                if (keyboard->MakeCode != KEYBOARD_OVERRUN_MAKE_CODE) {
                    u8 extension = keyboard->Flags & RI_KEY_E0 ? 0xE0 : keyboard->Flags & RI_KEY_E1 ? 0xE1 : 0x00;
                    u16 scan_code = (extension << 8) | (keyboard->MakeCode & 0x7F);
                    
                    b32 control_down = (GetKeyState(VK_CONTROL) < 0);
                    
                    switch (scan_code) {
                        case 0x0011: {      // W
                            if (control_down) {
                                cam_up = key_down;
                            } else {
                                cam_up = 0;
                                move_forward = key_down;
                            }
                        } break;
                        
                        case 0x001F: {      // S
                            if (control_down) {
                                cam_down = key_down;
                            } else {
                                cam_down = 0;
                                move_back = key_down;
                            }
                        } break;
                        
                        case 0x001E: {      // A
                            if (control_down) {
                                cam_left = key_down;
                            } else {
                                cam_left = 0;
                                strafe_left = key_down;
                            }
                        } break;
                        
                        case 0x0020: {      // D
                            if (control_down) {
                                cam_right = key_down;
                            } else {
                                cam_right = 0;
                                strafe_right = key_down;
                            }
                        } break;
                        
                        case 0x0001: {      // Escape
                            if (g_mouse_captured) {
                                g_mouse_captured = false;
                                ShowCursor(true);
                            }
                        } break;
                    }
                }
            } else if (input->header.dwType == RIM_TYPEMOUSE) {
                RAWMOUSE *mouse = &input->data.mouse;
                
                if (mouse->usButtonFlags & RI_MOUSE_BUTTON_1_UP) { // @hack
                    if (!g_mouse_captured) {
                        ShowCursor(false);
                        win32_capture_mouse(hwnd);
                    }
                }
                
                if (mouse->usButtonFlags & RI_MOUSE_WHEEL) {
                    short wheel = (short)mouse->usButtonData;
                    f32 wheel_delta = (f32)wheel / (f32)WHEEL_DELTA;
                    map_cam.z -= wheel_delta * MOUSE_SCROLL_SENSITIVITY;
                    map_cam.z = max(map_cam.z,1);
                }
                
                if (g_mouse_captured) {
                    s32 movex = mouse->lLastX;
                    turn_amount = ((f32)movex * MOUSE_SENSITIVITY);
                    win32_capture_mouse(hwnd);
                }
            }
            
            return 0;
        }
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
win32_create_bitmap (Bitmap *data) {
    BITMAPINFOHEADER header = {0};
    header.biSize = sizeof(header);
    header.biWidth = data->width;
    header.biHeight = data->height; // @note: negate for top-down
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    return (BITMAPINFO){header};
}

int
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    //- @note: Platform setup
    perm_arena = arena_alloc();
    frame_arena = arena_alloc();
    level_arena = arena_alloc();
    
    Win32_Data platform = win32_create_window(hInstance);
    
    // @note: Seed renderer
    time_t seed = time(0);
    srand((u32)seed);
    
    // @note: Register for input
    RAWINPUTDEVICE input_devices[2];
    
    // Keyboard
    input_devices[0].usUsagePage = 0x01;
    input_devices[0].usUsage = 0x06;
    input_devices[0].dwFlags = 0;
    input_devices[0].hwndTarget = 0;
    
    // Mouse
    input_devices[1].usUsagePage = 0x01;
    input_devices[1].usUsage = 0x02;
    input_devices[1].dwFlags = 0;
    input_devices[1].hwndTarget = 0;
    
    if (RegisterRawInputDevices(input_devices, array_count(input_devices), sizeof(input_devices[0])) == FALSE) {
        OutputDebugString("Unable to register input devices\n");
    }
    //win32_capture_mouse(platform.hwnd);
    //ShowCursor(false);
    
    // @note: Font setup
    //String8 font_path = str8_lit("W:/assets/dumb/fonts/Envy Code R PR7/Envy Code R.ttf");
    //String8 font_path = str8_lit("W:/assets/dumb/fonts/Retro Gaming.ttf");
    
    // @note: Timing
    LARGE_INTEGER frequency, start_time, end_time, elapsed_microseconds = {0};
    QueryPerformanceFrequency(&frequency);
    
    // @note: Create bitmap
    Bitmap *bitmap = r_get_framebuffer();
    bitmap->width = RESOLUTION_W;
    bitmap->height = RESOLUTION_H;
    bitmap->pixels = arena_pushn(perm_arena, u32, bitmap->width * bitmap->height);
    Quad2D screen_rect = {0};
    screen_rect.p1 = v2((f32)bitmap->width, (f32)bitmap->height);
    screen_rect.p2 = v2((f32)bitmap->width, 0);
    platform.bitmap = win32_create_bitmap(bitmap);
    
    //- @note: Game setup
    
    Entity player = {0};
    player.height = 15;
    player.rotation_angle = 0;
    player.radius = 20.f;
    player.pos.x = 0;
    player.pos.y = 0;
    player.curr_sector = 0;
    
    // I wish I had made a level editor
    // Load map data 
    Map test_level = map_load(perm_arena, str8_lit("w:/code/dumb/level.json"));
    map_cam = v3(0, 0, 50);
    
    //- @note: Main loop
    QueryPerformanceCounter(&start_time);
    for (;g_game_running;) {
        arena_clear(frame_arena);
        
        //- @note: Message loop
        for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
            if (msg.message == WM_QUIT) {
                g_game_running = false;
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
        
        Sector *player_sector = &test_level.sectors[player.curr_sector];
        
        player.rotation_angle -= turn_amount;
        player.rotation_angle = fmod_cycling(player.rotation_angle, 2 * M_PI32);
        turn_amount = 0;
        
        // @todo: There has got to be a better/cleaner/faster way to calculate movement + dir for both of these things
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
        
        Vec2 map_cam_dir = {0};
        if (cam_up)    map_cam_dir.y += 1;
        if (cam_down)  map_cam_dir.y -= 1;
        if (cam_left)  map_cam_dir.x -= 1;
        if (cam_right) map_cam_dir.x += 1;
        if (v2len(map_cam_dir) > 1)
            map_cam_dir = v2norm(map_cam_dir);
        Vec2 map_cam_v2 = v2add(dv3(map_cam), v2muls(map_cam_dir, CAM_MOVE_SPEED * dt));
        map_cam.x = map_cam_v2.x;
        map_cam.y = map_cam_v2.y;
        
        // @todo: Can we avoid polling every frame?
        update_current_sector(&player, &test_level);
        
        //- @note: Render
        r_sector(&test_level, player_sector, &player, screen_rect);
        //r_map(test_level, map_cam, player, true);
        
        // @todo: Preserve aspect ratio
        StretchDIBits(
                      platform.win_dc,
                      0, 0,
                      g_window_width,
                      g_window_height,
                      0, 0,
                      bitmap->width,
                      bitmap->height,
                      bitmap->pixels,
                      &platform.bitmap,
                      DIB_RGB_COLORS,
                      SRCCOPY
                      );
        
#if 0
        f32 fps = 1.f / dt;
        OutputDebugString((LPCSTR)str8_pushf(frame_arena, "FPS: %f\n", fps).str);
#endif
    }
    
    return 0;
}