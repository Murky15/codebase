#include <Windows.h>

#include "game.h"
#include "game.c"

#define RESOLUTION_W 640
#define RESOLUTION_H 360

#define MOUSE_SENSITIVITY 0.01f
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
                             "Voodoo (Working title), codename: \"Dumb\" | dev build",
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
    
    // @note: Timing
    LARGE_INTEGER frequency, start_time, end_time, elapsed_microseconds = {0};
    QueryPerformanceFrequency(&frequency);
    
    // @note: Create bitmap
    Bitmap *bitmap = r_get_framebuffer();
    bitmap->width = RESOLUTION_W;
    bitmap->height = RESOLUTION_H;
    bitmap->pixels = arena_pushn(perm_arena, u32, bitmap->width * bitmap->height);
    Range initial_bounds = v2(0, (f32)bitmap->width);
    platform.bitmap = win32_create_bitmap(bitmap);
    
    Game_Memory_Package game_memory = {perm_arena, frame_arena, level_arena};
    game_init(game_memory);
    
    //- @note: Main loop
    QueryPerformanceCounter(&start_time);
    for (;g_game_running;) {
        arena_clear(frame_arena);
        
        for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
            if (msg.message == WM_QUIT) {
                g_game_running = false;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        
        QueryPerformanceCounter(&end_time);
        elapsed_microseconds.QuadPart = end_time.QuadPart - start_time.QuadPart;
        f32 dt = (f32)((f32)elapsed_microseconds.QuadPart / (f32)frequency.QuadPart);
        start_time = end_time;
        
        Game_Tick_Package tick = {dt, ??}
        game_tick(game_memory, tick);
        game_render(game_memory);
        
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