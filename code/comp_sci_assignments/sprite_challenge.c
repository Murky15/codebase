#include "base/include.h"
#include "base/include.c"

#include <windows.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

LRESULT
win32_window_proc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CLOSE: PostQuitMessage(0); return 0;
    } 
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
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

    b32 running = true;
    for (;running;) {
        for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
            if (msg.message == WM_QUIT) {
                running = false;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        
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