/*
Naive zero dependency C implementation of Conway's game of life.
Performance and fancy tricks are not the goal here, this was just supposed to be 
a quick and simple implementation to learn the basics of cellular automata.
*/

#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#define SIM_TICK_RATE_MS 100
#define NUM_CELLS_X 50
#define NUM_CELLS_Y 50
#define CELL_WIDTH  50
#define CELL_HEIGHT 50
int grid_width = CELL_WIDTH * NUM_CELLS_X;
int grid_height = CELL_HEIGHT * NUM_CELLS_Y;

HDC hdc;
UINT timer_handle = -1;
float scale = 1;
bool cell_states[NUM_CELLS_X * NUM_CELLS_Y];

LRESULT APIENTRY 
WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    PAINTSTRUCT ps; 
    RECT rc; 
    switch (message) 
    { 
        case WM_CREATE: {
            hdc = GetDC(hwnd);
            SetROP2(hdc, R2_NOT);
            SetGraphicsMode(hdc, GM_ADVANCED);
            SetMapMode(hdc, MM_LOENGLISH);
            SetTimer(hwnd, timer_handle = 1, SIM_TICK_RATE_MS, NULL);
        }
        return 0L;
        
        case WM_TIMER: {
            
        }
        return 0L;
        
        case WM_MOUSEWHEEL: {
            // @todo: This is kinda jank on my trackpad
            short wheelDelta = (short)GET_WHEEL_DELTA_WPARAM(wParam);
            if (wheelDelta > 0)
                scale *= 1.1f;
            else if (wheelDelta < 0)
                scale *= 0.9f;
            if (fabs(wheelDelta)) {
                XFORM op = {0};
                op.eM11 = scale;
                op.eM22 = scale;
                SetWorldTransform(hdc, &op);
                // @todo: Super unoptimized
                RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
            }
        }
        return 0L;
        
        case WM_MOUSEMOVE: {
            
        }
        return 0L;
        
        case WM_PAINT: {
            BeginPaint(hwnd, &ps);
            // Draw Grid
            SelectObject(hdc, GetStockObject(HOLLOW_BRUSH)); 
            GetClientRect(hwnd, &rc);
            DPtoLP(hdc, (LPPOINT)&rc, 2);
            
            for (int y = rc.bottom; y < rc.bottom + grid_height; y += CELL_HEIGHT) {
                if (y < rc.top) {
                    MoveToEx(hdc, 0, y, NULL);
                    int draw_dist = grid_width > rc.right ? rc.right : grid_width;
                    LineTo(hdc, draw_dist, y);
                }
            }
            for (int x = 0; x < grid_width; x += CELL_WIDTH) {
                if (x < rc.right) {
                    MoveToEx(hdc, x, rc.bottom, NULL);
                    int real_grid_height = grid_height + rc.bottom; // Windows GDI makes absolutely zero sense
                    int draw_dist = real_grid_height > rc.top ? rc.top : real_grid_height;
                    LineTo(hdc, x, draw_dist);
                }
            }
            EndPaint(hwnd, &ps);
        }
        return 0L;
        
        case WM_DESTROY: 
        PostQuitMessage(0); 
        return 0L; 
    } 
    return DefWindowProc(hwnd, message, wParam, lParam); 
}

int WINAPI 
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow) {
    char CLASS_NAME[]  = "Window Class";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = CLASS_NAME;
    wc.style         = CS_OWNDC;
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(
                               0,                              
                               CLASS_NAME,                    
                               "Conway's Game of Life",   
                               WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,            
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
    
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}
