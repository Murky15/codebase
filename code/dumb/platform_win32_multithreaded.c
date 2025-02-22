#include "game.c"

#include <Windows.h>

#define RESOLUTION_W 640
#define RESOLUTION_H 360

#define MOUSE_SENSITIVITY 0.70f
#define MOUSE_SCROLL_SENSITIVITY 0.8f
#define CAM_MOVE_SPEED 200.f

#define GAME_MEMORY_SIZE Gigabytes(1)

typedef struct Platform_Timing_Info {
    f32 tick_hz, render_hz;
} Platform_Timing_Info;

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

// @todo: Technically while this is still protected by the game_memory mutex, it's sloppy keeping this loose
global u64 last_game_tick;

function void
win32_capture_mouse (void) {
    platform.mouse_captured = true;
    RECT cr;
    GetClientRect(platform.hwnd, &cr);
    POINT middle  = {cr.right/2, cr.bottom/2};
    ClientToScreen(platform.hwnd, &middle);
    SetCursorPos(middle.x, middle.y);
}

function u64
win32_get_perf_frequency (void) {
    local_persist threadvar LARGE_INTEGER freq;
    if (freq.QuadPart == 0)
        QueryPerformanceFrequency(&freq);
        
    return freq.QuadPart;
}

function u64 
win32_query_clock (void) {
    LARGE_INTEGER tick;
    QueryPerformanceCounter(&tick);
    
    return tick.QuadPart;
}

function f32
win32_get_elapsed_ms (u64 t1, u64 t2) {
    u64 freq = win32_get_perf_frequency();
    u64 elapsed_ms = (t2 - t1) * 1000;
    
    return (f32)elapsed_ms / (f32)freq;    
}

function u64
win32_ms_to_tick_interval (f32 ms) {
    u64 freq = win32_get_perf_frequency();
    u64 ticks = ms * freq;
    ticks /= 1000;
    
    return ticks; 
}

function LRESULT
win32_game_window_proc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (hwnd == platform.hwnd) { // @todo: Preserve aspect ratio
        switch (uMsg) {
            case WM_CLOSE: PostQuitMessage(0); return 0;
            
            case WM_INPUT: {
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

// @note/todo: These loops run so fast that having the render wait for the platform and memory mutexes 
// Cause it to hitch and stall the other threads. Because the render thread ONLY READS from the data 
// this is PROBABLY safe to do. The only problem would be if one of the platform values changes so we will
// need to revisit this. Same concept applies to the Game_Input and Platform structures. Each structure is 
// only written to by one thread and only read by another (somtimes through a copy) so we may be able to get away with 
// this without the need for mutexes.

function DWORD WINAPI
win32_game_tick (LPVOID param) {
    f32 tick_hz = *(f32*)param;
    f32 seconds_per_tick = (1.f / tick_hz);
    f32 ms_per_tick = seconds_per_tick * 1000.f;
     
    while (1) {        
        u64 start = win32_query_clock();
        last_game_tick = start; // @megahack
        game_tick(game_memory, game_input, seconds_per_tick);
        game_input.turn_amount = 0; // @megahack 2
        
        u64 end = win32_query_clock();
        f32 elapsed = win32_get_elapsed_ms(start, end);
        f32 time_diff = ms_per_tick - elapsed;
        if (time_diff > 0) {
            Sleep(time_diff);
        } else {
            // @todo: Log frame miss
        }
        u64 super_end = win32_query_clock();
        //fprintf(stderr, "Time to tick (ms): %f\n", win32_get_elapsed_ms(start, super_end));  
    }
}

function DWORD WINAPI
win32_game_render (LPVOID param) {
    Platform_Timing_Info *timing = (Platform_Timing_Info*)param;
    f32 render_hz = timing->render_hz;
    f32 tick_hz = timing->tick_hz;
    f32 ms_per_frame = (1.f / render_hz) * 1000.f;
    f32 ms_per_tick = (1.f / tick_hz) * 1000.f;
    
    u64 game_tick_duration = win32_ms_to_tick_interval(ms_per_tick);
    while (1) {
        u64 start = win32_query_clock();
        u64 predicted_end_game_tick = last_game_tick + game_tick_duration;
        assert(last_game_tick < start < predicted_end_game_tick);
        f32 t = norm((f64)start, (f64)last_game_tick, (f64)predicted_end_game_tick);
        game_render(game_memory, t);
        
        Bitmap *bitmap = r_get_framebuffer();
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
        
        u64 end = win32_query_clock();
        f32 elapsed = win32_get_elapsed_ms(start, end);
        f32 time_diff = ms_per_frame - elapsed;
        if (time_diff > 0) {
            Sleep(time_diff);
        } else {
            // @todo: Log frame miss
        }
        u64 super_end = win32_query_clock();
        //fprintf(stderr, "Time to render (ms): %f\n", win32_get_elapsed_ms(start, super_end));   
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
    
    //- @note: Query monitor info (@todo: We will need to call this again if the game swaps monitors)
    DEVMODE monitor_info = {0};
    monitor_info.dmSize = sizeof(monitor_info);
    assert(EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &monitor_info));
    
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

    f32 game_tick_hz = 30.f;
    f32 game_render_hz = monitor_info.dmDisplayFrequency / 2.f; 
    Platform_Timing_Info timing = {.tick_hz = game_tick_hz, .render_hz = game_render_hz};
    CreateThread(NULL, 0, win32_game_tick, &game_tick_hz, 0, 0);
    CreateThread(NULL, 0, win32_game_render, &timing, 0, 0);
    
    //- @note: Input loop
    for (MSG msg; GetMessage(&msg, 0, 0, 0);) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }       
    
    ExitProcess(0);
}