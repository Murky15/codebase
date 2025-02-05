#include "game.c"

#include <Windows.h>

#define RESOLUTION_W 640
#define RESOLUTION_H 360

#define MOUSE_SENSITIVITY 0.01f
#define MOUSE_SCROLL_SENSITIVITY 0.8f
#define CAM_MOVE_SPEED 200.f

#define GAME_MEMORY_SIZE Kilobytes(4)

global struct {
    Arena *arena;
    
    HINSTANCE hInstance;
    HWND hwnd;
    HDC win_dc;
    BITMAPINFO bitmap;
    
    HWND tool_window;
    HDC dev_dc;
    HGLRC dev_gl_ctx;
    
    u32 window_width;
    u32 window_height;
    b32 mouse_captured;
} platform;

global Game_Input_Package game_input;
global Game_Memory_Package game_memory;

global HANDLE platform_mutex;
global HANDLE game_input_mutex;
global HANDLE game_memory_mutex;

function void
win32_capture_mouse (void) {
    platform.mouse_captured = true;
    RECT cr;
    GetClientRect(platform.hwnd, &cr);
    POINT middle  = {cr.right/2, cr.bottom/2};
    ClientToScreen(platform.hwnd, &middle);
    SetCursorPos(middle.x, middle.y);
}

function LRESULT
win32_game_window_proc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (hwnd == platform.hwnd) { // @todo: Be careful here...
        switch (uMsg) {
            case WM_CLOSE: PostQuitMessage(0); return 0;
            
            case WM_INPUT: {
                WaitForSingleObject(game_input_mutex, INFINITE);
                u32 size;
                GetRawInputData((HRAWINPUT)lParam, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
                LPBYTE buff = malloc(size);
                if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buff, &size, sizeof(RAWINPUTHEADER)) != size)
                    OutputDebugString("GetRawInputData does not return correct size !\n");
                
                RAWINPUT *input = (RAWINPUT*)buff;
                if (input->header.dwType == RIM_TYPEKEYBOARD) {
                    RAWKEYBOARD *keyboard = &input->data.keyboard;
                    b32 key_down = (keyboard->Flags & RI_KEY_BREAK) == 0;
                    if (keyboard->MakeCode != KEYBOARD_OVERRUN_MAKE_CODE) {
                        u8 extension = keyboard->Flags & RI_KEY_E0 ? 0xE0 : keyboard->Flags & RI_KEY_E1 ? 0xE1 : 0x00;
                        u16 scan_code = (extension << 8) | (keyboard->MakeCode & 0x7F);
                        
                        switch (scan_code) {
                            case 0x0011: {      // W
                                    game_input.move_forward = key_down;
                            } break;
                            
                            case 0x001F: {      // S
                                    game_input.move_back = key_down;
                            } break;
                            
                            case 0x001E: {      // A
                                    game_input.strafe_left = key_down;
                            } break;
                            
                            case 0x0020: {      // D
                                    game_input.strafe_right = key_down;
                            } break;
                            
                            case 0x0001: {      // Escape
                                if (platform.mouse_captured) {
                                    platform.mouse_captured = false;
                                    ShowCursor(true);
                                }
                            } break;
                        }
                    }
                } else if (input->header.dwType == RIM_TYPEMOUSE) {
                    RAWMOUSE *mouse = &input->data.mouse;
                    
                    // @todo: Only capture the mouse if it is in client rect
                    if (mouse->usButtonFlags & RI_MOUSE_BUTTON_1_UP) { // @hack
                        if (!platform.mouse_captured) {
                            ShowCursor(false);
                            win32_capture_mouse();
                        }
                    }
#if 0
                    if (mouse->usButtonFlags & RI_MOUSE_WHEEL) {
                        short wheel = (short)mouse->usButtonData;
                        f32 wheel_delta = (f32)wheel / (f32)WHEEL_DELTA;
                        map_cam.z -= wheel_delta * MOUSE_SCROLL_SENSITIVITY;
                        map_cam.z = max(map_cam.z,1);
                    }
#endif
                    if (platform.mouse_captured) {
                        s32 movex = mouse->lLastX;
                        game_input.turn_amount = ((f32)movex * MOUSE_SENSITIVITY);
                        win32_capture_mouse();
                    }
                }
                free(buff);
                ReleaseMutex(game_input_mutex);
                return 0;
            }
        }
    } else if (hwnd == platform.tool_window) {
        switch (uMsg) {
            case WM_SIZE: {
                return 0;
            }
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

function DWORD WINAPI
win32_game_tick (LPVOID param) {
    LARGE_INTEGER freq;
    LARGE_INTEGER start, end, elapsed;
    QueryPerformanceFrequency(&freq);
    f32 tick_hz = *(f32*)param;
    f32 seconds_per_tick = (1.f / tick_hz);
    f32 ms_per_tick = seconds_per_tick * 1000.f;
    
    HANDLE needed_objects[] = {game_input_mutex, game_memory_mutex}; 
    while (1) {
        QueryPerformanceCounter(&start);
        WaitForMultipleObjects(array_count(needed_objects), needed_objects, true, INFINITE);
        {
            game_tick(game_memory, game_input, seconds_per_tick);
            game_input.turn_amount = 0; // @megahack
        }
        for (u32 i = 0; i < array_count(needed_objects); ++i) {
            ReleaseMutex(needed_objects[i]);
        }
        
        QueryPerformanceCounter(&end);
        elapsed.QuadPart = end.QuadPart - start.QuadPart;
        elapsed.QuadPart *= 1000;
        f32 elapsed_ms = (f32)elapsed.QuadPart / (f32)freq.QuadPart;
        f32 time_diff = ms_per_tick - elapsed_ms;
        if (time_diff > 0) {
            Sleep(time_diff);
        } else {
            // @todo: Log frame miss
        }
    }
}

function DWORD WINAPI
win32_game_render (LPVOID param) {
    LARGE_INTEGER freq;
    LARGE_INTEGER start, end, elapsed;
    QueryPerformanceFrequency(&freq);
    f32 render_hz = *(f32*)param;
    f32 ms_per_frame = (1.f / render_hz) * 1000.f;
    
    HANDLE needed_objects[] = {platform_mutex, game_memory_mutex}; 
    while (1) {
        QueryPerformanceCounter(&start);
        WaitForMultipleObjects(array_count(needed_objects), needed_objects, true, INFINITE);
        {
            game_render(game_memory);
            Bitmap *bitmap = r_get_framebuffer();
            // @todo: Preserve aspect ratio
            StretchDIBits(
                          platform.win_dc,
                          0, 0,
                          platform.window_width,
                          platform.window_height,
                          0, 0,
                          bitmap->width,
                          bitmap->height,
                          bitmap->pixels,
                          &platform.bitmap,
                          DIB_RGB_COLORS,
                          SRCCOPY
                          );
        }
        for (u32 i = 0; i < array_count(needed_objects); ++i) {
            ReleaseMutex(needed_objects[i]);
        }
        
        QueryPerformanceCounter(&end);
        elapsed.QuadPart = end.QuadPart - start.QuadPart;
        elapsed.QuadPart *= 1000;
        f32 elapsed_ms = (f32)elapsed.QuadPart / (f32)freq.QuadPart;
        f32 time_diff = ms_per_frame - elapsed_ms;
        if (time_diff > 0) {
            Sleep(time_diff);
        } else {
            // @todo: Log frame miss
        }        
    }
}

int
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    //- @note: Init
    platform.arena = arena_alloc();
    platform.hInstance = hInstance;
    platform.window_width = 1280;
    platform.window_height = 720;
    
    WNDCLASS wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = win32_game_window_proc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = "default window class";
    RegisterClass(&wc);
    
    RECT dim = {0, 0, platform.window_width, platform.window_height};
    AdjustWindowRect(&dim, WS_OVERLAPPEDWINDOW, 0);
    platform.hwnd = CreateWindow(
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
    if (!platform.hwnd) {
        OutputDebugString("Window creation failed!\n");
        return 1;
    }
    platform.win_dc = GetDC(platform.hwnd);
    
    //- @note: Register for input
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
    
    //- @note: Create bitmap
    Bitmap *bitmap = r_get_framebuffer();
    bitmap->width = RESOLUTION_W;
    bitmap->height = RESOLUTION_H;
    bitmap->pixels = arena_pushn(platform.arena, u32, bitmap->width * bitmap->height);
    Range view_bounds = v2(0, (f32)bitmap->width);
    
    BITMAPINFOHEADER header = {0};
    header.biSize = sizeof(header);
    header.biWidth = bitmap->width;
    header.biHeight = bitmap->height; // @note: negate for top-down
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    platform.bitmap = (BITMAPINFO){header};
    
    //- @note: Initialize developer tools
    
    //- @note: Initialize game
    void *buff = arena_pushn(platform.arena, u8, GAME_MEMORY_SIZE);
    game_memory = (Game_Memory_Package){buff, GAME_MEMORY_SIZE};
    game_init(game_memory, view_bounds);    

    //- @note: Threading, Timing, & Scheduling setup
    assert(SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS) != 0);
    timeBeginPeriod(1);
    f32 game_tick_hz = 50;
    f32 game_render_hz = 72; // @todo: How can we determine the refresh rate of the monitor 
    assert(CreateThread(NULL, 0, win32_game_tick, &game_tick_hz, 0, 0));
    assert(CreateThread(NULL, 0, win32_game_render, &game_render_hz, 0, 0));
    
    //- @note: Input loop
    for (MSG msg; GetMessage(&msg, 0, 0, 0);) {
        WaitForSingleObject(platform_mutex, INFINITE);
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        ReleaseMutex(platform_mutex);
    }       
    
    ExitProcess(0);
}