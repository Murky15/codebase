#include <windows.h>

typedef struct Win32_Data {
    HINSTANCE hInstance;
    HWND hwnd;
    HDC dc;
} Win32_Data;

function Win32_Data
win32_create_window (u32 width, u32 height, char *title, HINSTANCE hInstance) {
    Win32_Data result = {0};
    
    WNDCLASS wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = "window_class";
    RegisterClass(&wc);
    
    RECT dim = {0, 0, width, height};
    AdjustWindowRect(&dim, WS_OVERLAPPEDWINDOW, 0);
    HWND hwnd = CreateWindow(wc.lpszClassName,
                             title,
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             dim.right - dim.left,
                             dim.bottom - dim.top,
                             0, 0
                             hInstance,
                             0);
    if (!hwnd) {
        OutputDebugString("Window creation failed!\n");
        return result;
    }
    
    result.hInstance = hInstance;
    resullt.hwnd = hwnd;
    result.dc = GetDC(hwnd);
    return result;
}

int
WinMain (HINSTANCE hInstance, 
         HINSTANCE hPrevInstance, 
         LPSTR lpCmdLine, 
         int nCmdShow) {
    Win32_Data win32 = win32_create_window(800, 800, "Gianni's Game of Life", hInstance);
    return 0;
} 