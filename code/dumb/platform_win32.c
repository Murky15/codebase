#include "game.c"

#include <Windows.h>
//#include "file/png.c"
//#include "dev.c"

#define RESOLUTION_W 640
#define RESOLUTION_H 360

#define GAME_MEMORY_SIZE Kilobytes(4)

#define MOUSE_SENSITIVITY 0.01f
#define MOUSE_SCROLL_SENSITIVITY 0.8f
#define CAM_MOVE_SPEED 200.f

typedef struct Win32_Data {
    Arena *perm_arena;
    Arena *frame_arena;
    
    HINSTANCE hInstance;
    HWND hwnd;
    HDC win_dc;
    BITMAPINFO bitmap;
    
    HWND tool_window;
    HDC dev_dc;
    HGLRC dev_gl_ctx;
} Win32_Data;
global Win32_Data platform;

global int window_width = 1280;
global int window_height = 720;
global b32 game_running = true;
global b32 mouse_captured = false;

global Game_Input_Package game_input;
global b32 cam_up, cam_down, cam_left, cam_right;

function void
win32_capture_mouse (HWND hwnd) {
    mouse_captured = true;
    RECT cr;
    GetClientRect(hwnd, &cr);
    POINT middle  = {cr.right/2, cr.bottom/2};
    ClientToScreen(hwnd, &middle);
    SetCursorPos(middle.x, middle.y);
}

function LRESULT
win32_game_window_proc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (hwnd == platform.hwnd) { // @todo: Be careful here...
        switch (uMsg) {
            case WM_CLOSE: PostQuitMessage(0); return 0;
            
            case WM_INPUT: {
                u32 size;
                GetRawInputData((HRAWINPUT)lParam, RID_INPUT, 0, &size, sizeof(RAWINPUTHEADER));
                LPBYTE buff = arena_pushn(platform.frame_arena, BYTE, size);
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
                                    game_input.move_forward = key_down;
                                }
                            } break;
                            
                            case 0x001F: {      // S
                                if (control_down) {
                                    cam_down = key_down;
                                } else {
                                    cam_down = 0;
                                    game_input.move_back = key_down;
                                }
                            } break;
                            
                            case 0x001E: {      // A
                                if (control_down) {
                                    cam_left = key_down;
                                } else {
                                    cam_left = 0;
                                    game_input.strafe_left = key_down;
                                }
                            } break;
                            
                            case 0x0020: {      // D
                                if (control_down) {
                                    cam_right = key_down;
                                } else {
                                    cam_right = 0;
                                    game_input.strafe_right = key_down;
                                }
                            } break;
                            
                            case 0x0001: {      // Escape
                                if (mouse_captured) {
                                    mouse_captured = false;
                                    ShowCursor(true);
                                }
                            } break;
                        }
                    }
                } else if (input->header.dwType == RIM_TYPEMOUSE) {
                    RAWMOUSE *mouse = &input->data.mouse;
                    
                    // @todo: Only capture the mouse if it is in client rect
                    if (mouse->usButtonFlags & RI_MOUSE_BUTTON_1_UP) { // @hack
                        if (!mouse_captured) {
                            ShowCursor(false);
                            win32_capture_mouse(hwnd);
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
                    if (mouse_captured) {
                        s32 movex = mouse->lLastX;
                        game_input.turn_amount = ((f32)movex * MOUSE_SENSITIVITY);
                        win32_capture_mouse(hwnd);
                    }
                }
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

function Win32_Data
win32_create_game_window_and_init (HINSTANCE hInstance) {
    WNDCLASS wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = win32_game_window_proc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = "default window class";
    RegisterClass(&wc);
    
    RECT dim = {0, 0, window_width, window_height};
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
    result.perm_arena = arena_alloc();
    result.frame_arena = arena_alloc();
    
    return result;
}

function void
win32_create_dev_window (Win32_Data *win32) {
    HWND dev_window = CreateWindow("default window class",
                                   "VOODOO: DEVELOPER TOOLS",
                                   WS_OVERLAPPED | WS_VISIBLE | WS_THICKFRAME,
                                   CW_USEDEFAULT,
                                   CW_USEDEFAULT,
                                   800,
                                   600,
                                   0,0,
                                   win32->hInstance,
                                   0
                                   );
    if (dev_window) {
        win32->tool_window = dev_window;
        win32->dev_dc = GetDC(dev_window);
        
        PIXELFORMATDESCRIPTOR pfd =
        {
            sizeof(PIXELFORMATDESCRIPTOR),
            1,
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA,
            32,
            0, 0, 0, 0, 0, 0,
            0,
            0,
            0,
            0, 0, 0, 0,
            24,
            8,
            0,
            PFD_MAIN_PLANE,
            0,
            0, 0, 0
        };
        
        int fid = ChoosePixelFormat(win32->dev_dc, &pfd);
        if (fid != 0) {
            SetPixelFormat(win32->dev_dc, fid, &pfd);
            win32->dev_gl_ctx = wglCreateContext(win32->dev_dc);
            wglMakeCurrent(win32->dev_dc, win32->dev_gl_ctx);
        }
        
        RECT dim;
        GetClientRect(dev_window, &dim);
        //dev_renderer_calibrate(dim.right, dim.bottom);
        
    } else {
        OutputDebugString("Failure to create developer window!\n");
    }
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
    //- @note: Init
    platform = win32_create_game_window_and_init(hInstance);
    win32_create_dev_window(&platform);
    
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
    
    //- @note: Timing
    LARGE_INTEGER frequency, start_time, end_time, elapsed_microseconds = {0};
    QueryPerformanceFrequency(&frequency);
    
    //- @note: Create bitmap
    Bitmap *bitmap = r_get_framebuffer();
    bitmap->width = RESOLUTION_W;
    bitmap->height = RESOLUTION_H;
    bitmap->pixels = arena_pushn(platform.perm_arena, u32, bitmap->width * bitmap->height);
    Range view_bounds = v2(0, (f32)bitmap->width);
    platform.bitmap = win32_create_bitmap(bitmap);
    
    //- @note: Initialize developer tools
    
    //- @note: Initialize game
    void *buff = arena_pushn(platform.perm_arena, u8, GAME_MEMORY_SIZE);
    Game_Memory_Package game_memory = {buff, GAME_MEMORY_SIZE};
    game_init(game_memory, view_bounds);
    
    //- @note: Main loop
    QueryPerformanceCounter(&start_time);
    for (;game_running;) {
        arena_clear(platform.frame_arena);
        
        for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
            if (msg.message == WM_QUIT) {
                game_running = false;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        
        
        QueryPerformanceCounter(&end_time);
        elapsed_microseconds.QuadPart = end_time.QuadPart - start_time.QuadPart;
        f32 dt = (f32)((f32)elapsed_microseconds.QuadPart / (f32)frequency.QuadPart);
        start_time = end_time;
        
        SwapBuffers(platform.dev_dc);
        
        game_tick(game_memory, game_input, dt);
        game_render(game_memory);
        // @todo: Preserve aspect ratio
        StretchDIBits(
                      platform.win_dc,
                      0, 0,
                      window_width,
                      window_height,
                      0, 0,
                      bitmap->width,
                      bitmap->height,
                      bitmap->pixels,
                      &platform.bitmap,
                      DIB_RGB_COLORS,
                      SRCCOPY
                      );
        
        
        game_input.turn_amount = 0;
#if 0
        f32 fps = 1.f / dt;
        OutputDebugString((LPCSTR)str8_pushf(frame_arena, "FPS: %f\n", fps).str);
#endif
    }
    return 0;
}