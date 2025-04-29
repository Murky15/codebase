#include "base/include.h"
#include "base/include.c"

#include <windows.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define NUM_BALLS 10
#define BOX_SPEED 100.f
#define BALL_SPEED 5.f

typedef struct Box {
    Vec2i pos;
    Vec2i dim;
} Box;

typedef struct Ball {
    Vec2i pos;
    f32 radius;
} Ball;

global Box box;
global Ball balls[NUM_BALLS];

LRESULT
win32_window_proc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE: PostQuitMessage(0); return 0;
        case WM_KEYDOWN: {
            if (wParam == VK_UP)
                box.dim.y += 5;
            if (wParam == VK_DOWN && box.dim.y >= 30)
                box.dim.y -= 5;
            if (wParam == VK_LEFT && box.dim.x >= 30)
                box.dim.x -= 5;
            if (wParam == VK_RIGHT)
                box.dim.x += 5;
            return 0;
        }
    } 
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void
put_pixel_at (u32 *pixels, Vec2i pos, Color c) {
    if (pos.x >= 0 && pos.y >= 0 && pos.x < WINDOW_WIDTH && pos.y < WINDOW_HEIGHT)
        pixels[pos.y * WINDOW_WIDTH + pos.x] = (c.r << 16 | c.g << 8 | c.b);
}

void
update_simulation (Box box, f32 dt) {
    
}

void 
draw_simulation (u32 *pixels, Box box) {
    for (u32 x = 0; x < box.dim.x; ++x) {
        put_pixel_at(pixels, v2i(box.pos.x + x, box.pos.y), Color_White);
        put_pixel_at(pixels, v2i(box.pos.x + x, box.pos.y + box.dim.y), Color_White);
    }
    for (u32 y = 0; y < box.dim.y; ++y) {
        put_pixel_at(pixels, v2i(box.pos.x, box.pos.y + y), Color_White);
        put_pixel_at(pixels, v2i(box.pos.x + box.dim.x, box.pos.y + y), Color_White);
    }
}

int WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow) {
    
    WNDCLASS wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = win32_window_proc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = "default window class";
    RegisterClass(&wc);
    
    RECT dim = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    AdjustWindowRect(&dim, WS_OVERLAPPEDWINDOW, 0);
    HWND hwnd = CreateWindow(
                             wc.lpszClassName,
                             "Mr. Soni programming challenge",
                             (WS_OVERLAPPEDWINDOW | WS_VISIBLE) ^ WS_THICKFRAME,
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
        return 1;
    }
    HDC win_dc = GetDC(hwnd);
    
    BITMAPINFOHEADER header = {0};
    header.biSize = sizeof(header);
    header.biWidth = WINDOW_WIDTH;
    header.biHeight = WINDOW_HEIGHT; // @note: negate for top-down
    header.biPlanes = 1;
    header.biBitCount = 32;
    header.biCompression = BI_RGB;
    BITMAPINFO bitmap = (BITMAPINFO){header};
    
    Arena *arena = arena_alloc();
    void *pixels = arena_pushn(arena, u32, WINDOW_WIDTH * WINDOW_HEIGHT);
    
    // Initialize simulation
    box.dim = v2i(30, 30);
    box.pos = v2i(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
    
    LARGE_INTEGER frequency, start_time, end_time, elapsed_microseconds = {0};
    QueryPerformanceFrequency(&frequency);
    
    b32 running = true;
    QueryPerformanceCounter(&start_time);
    for (;running;) {
        for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
            if (msg.message == WM_QUIT) {
                running = false;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        
        QueryPerformanceCounter(&end_time);
        elapsed_microseconds.QuadPart = end_time.QuadPart - start_time.QuadPart;
        f32 dt = (f32)((f32)elapsed_microseconds.QuadPart / (f32)frequency.QuadPart);
        start_time = end_time;
        
        update_simulation(box, dt);
        
        memory_zero(pixels, sizeof(u32) * WINDOW_WIDTH * WINDOW_HEIGHT);
        draw_simulation((u32*)pixels, box);
        
        StretchDIBits(
                      win_dc,
                      0, 0,
                      WINDOW_WIDTH,
                      WINDOW_HEIGHT,
                      0, 0,
                      WINDOW_WIDTH,
                      WINDOW_HEIGHT,
                      pixels,
                      &bitmap,
                      DIB_RGB_COLORS,
                      SRCCOPY
                      );
        
    }
    return 0;
}