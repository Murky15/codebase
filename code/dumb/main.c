//~ @note: Unity build
//- @note: Headers
#include <Windows.h>
#include "base/include.h"

typedef struct Wall {
    Vec2 p0, p1;
    f32 height;
} Wall;


#include "renderer.h"
//- @note: Source
#include "base/include.c"
#include "renderer.c"

/*
@todo
- [ ] Optimize / profile render functions
- [ ] Asan / Libfuzzer
- [ ] sin/cos/tan table lookup: https://namoseley.wordpress.com/2015/07/26/sincos-generation-using-table-lookup-and-iterpolation/
*/

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
    Bitmap *bitmap = r_get_framebuffer();
    platform.bitmap = win32_create_bitmap(bitmap);
    
    //~ @note: Game setup
    
    Entity player = {0};
    player.rotation_angle = 0;
    player.radius = 10.f;
    player.pos.x = bitmap->width / 2.f;
    player.pos.y = bitmap->height / 2.f;
    
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
        
        f32 turn_amount = 1.5f;
        if (turn_left)  player.rotation_angle += turn_amount * dt;
        if (turn_right) player.rotation_angle -= turn_amount * dt;
        player.rotation_angle = fmod_cycling(player.rotation_angle, 2 * M_PI32);
        
        //- @note: Render
        r_clear();
        r_scene(player.pos, player.rotation_angle, &test_wall, 1);
        
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
    }
    
    return 0;
}