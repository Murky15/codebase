/*
Single file c implementation of conway's game of life.
Just meant to be a fast and ugly implementation. No fancy tricks or optimizations (yet)
*/

#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include <stdint.h>

typedef struct Mem_Block {
    void *mem;
    uint64_t size;
} Mem_Block;

// Measured in cells
static uint32_t canvas_width  = 800;
static uint32_t canvas_height = 800;

LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI ThreadProc(LPVOID);

int WINAPI 
wWinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[]  = L"Window Class";
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(
                               0,                              
                               CLASS_NAME,                    
                               L"Conway's Game of Life",   
                               WS_OVERLAPPEDWINDOW,            
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               NULL,         
                               NULL,       
                               hInstance,  
                               NULL       
                               );
    if (hwnd == NULL)
    {
        return 0;
    }
    ShowWindow(hwnd, nCmdShow);
    
    // Create game thread
    uint64_t mem_size = canvas_width*canvas_height*sizeof(uint8_t)*2;
    void *mem = HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, mem_size);
    Mem_Block game_memory = {mem, mem_size};
    DWORD tid;
    CreateThread(0, 0, ThreadProc, (LPVOID)&game_memory, 0, &tid);
    
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}

LRESULT CALLBACK 
WindowProc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg)
    {
        case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
        
        case WM_PAINT: { 
            PAINTSTRUCT ps;
            RECT rc;
            HDC dc = BeginPaint(hwnd, &ps);
            GetClientRect(hwnd, &rc);
            return 0;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

DWORD WINAPI 
ThreadProc (LPVOID game_memory) {
    OutputDebugString(L"Hello from the game thread\n");
    Mem_Block *memory = (Mem_Block*)game_memory;
    uint64_t frame_size = (memory->size/2);
    uint8_t *curr_frame = (uint8_t*)memory->mem;
    uint8_t *next_frame = (uint8_t*)memory->mem + frame_size;
    
    
    return 0;
}