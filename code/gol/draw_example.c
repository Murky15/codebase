#include <windows.h>


RECT rcCurrent = {0,0,20,20}; 
POINT aptStar[6] = {10,1, 1,19, 19,6, 1,6, 19,19, 10,1}; 
int X = 2, Y = -1, idTimer = -1; 
BOOL fVisible = FALSE; 
HDC hdc; 

LRESULT APIENTRY 
WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    PAINTSTRUCT ps; 
    RECT rc; 
    
    switch (message) 
    { 
        case WM_CREATE: 
        
        // Calculate the starting point.  
        
        GetClientRect(hwnd, &rc); 
        OffsetRect(&rcCurrent, rc.right / 2, rc.bottom / 2); 
        
        // Initialize the private DC.  
        
        hdc = GetDC(hwnd); 
        SetViewportOrgEx(hdc, rcCurrent.left, 
                         rcCurrent.top, NULL); 
        SetROP2(hdc, R2_NOT); 
        
        // Start the timer.  
        
        SetTimer(hwnd, idTimer = 1, 10, NULL); 
        return 0L; 
        
        case WM_DESTROY: 
        KillTimer(hwnd, 1); 
        PostQuitMessage(0); 
        return 0L; 
        
        case WM_SIZE: 
        switch (wParam) 
        { 
            case SIZE_MINIMIZED: 
            
            // Stop the timer if the window is minimized. 
            
            KillTimer(hwnd, 1); 
            idTimer = -1; 
            break; 
            
            case SIZE_RESTORED: 
            
            // Move the star back into the client area  
            // if necessary.  
            
            if (rcCurrent.right > (int) LOWORD(lParam)) 
            {
                rcCurrent.left = 
                (rcCurrent.right = 
                 (int) LOWORD(lParam)) - 20; 
            }
            if (rcCurrent.bottom > (int) HIWORD(lParam)) 
            {
                rcCurrent.top = 
                (rcCurrent.bottom = 
                 (int) HIWORD(lParam)) - 20; 
            }
            
            // Fall through to the next case.  
            
            case SIZE_MAXIMIZED: 
            
            // Start the timer if it had been stopped.  
            
            if (idTimer == -1) 
                SetTimer(hwnd, idTimer = 1, 10, NULL); 
            break; 
        } 
        return 0L; 
        
        case WM_TIMER: 
        
        // Hide the star if it is visible.  
        
        if (fVisible) 
            Polyline(hdc, aptStar, 6); 
        
        // Bounce the star off a side if necessary.  
        
        GetClientRect(hwnd, &rc); 
        if (rcCurrent.left + X < rc.left || 
            rcCurrent.right + X > rc.right) 
            X = -X; 
        if (rcCurrent.top + Y < rc.top || 
            rcCurrent.bottom + Y > rc.bottom) 
            Y = -Y; 
        
        // Show the star in its new position.  
        
        OffsetRect(&rcCurrent, X, Y); 
        SetViewportOrgEx(hdc, rcCurrent.left, 
                         rcCurrent.top, NULL); 
        fVisible = Polyline(hdc, aptStar, 6); 
        
        return 0L; 
        
        case WM_ERASEBKGND: 
        
        // Erase the star.  
        
        fVisible = FALSE; 
        return DefWindowProc(hwnd, message, wParam, lParam); 
        
        case WM_PAINT: 
        
        // Show the star if it is not visible. Use BeginPaint  
        // to clear the update region.  
        
        BeginPaint(hwnd, &ps); 
        if (!fVisible) 
            fVisible = Polyline(hdc, aptStar, 6); 
        EndPaint(hwnd, &ps); 
        return 0L; 
    } 
    return DefWindowProc(hwnd, message, wParam, lParam); 
} 

int WINAPI 
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow) {
    const char CLASS_NAME[]  = "Window Class";
    
    WNDCLASS wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(
                               0,                              
                               CLASS_NAME,                    
                               "Conway's Game of Life",   
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
    
    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    
    return 0;
}
