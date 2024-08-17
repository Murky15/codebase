#include <Windows.h>
#include <d3d11.h>

#include "base/include.h"
#include "base/include.c"

#define CANVAS_WIDTH  1280
#define CANVAS_HEIGHT 720

function LRESULT
Wndproc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_CLOSE: PostQuitMessage(0); return 0; break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

function HWND
win32_create_window (HINSTANCE hInstance) {
  WNDCLASS wc = {0};
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = Wndproc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = GetStockObject(WHITE_BRUSH);
  wc.lpszClassName = "d3d11_playground_wc";
  RegisterClass(&wc);

  RECT dim = {0, 0, CANVAS_WIDTH, CANVAS_HEIGHT};
  AdjustWindowRect(&dim, WS_OVERLAPPEDWINDOW, 0);
  HWND hwnd = CreateWindow(
    wc.lpszClassName,
    "D3D11 Playground",
    (WS_OVERLAPPEDWINDOW | WS_VISIBLE) ^ WS_THICKFRAME, // @todo: Handle window resizes
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    dim.right - dim.left,
    dim.bottom - dim.top,
    0, 0,
    hInstance,
    0
  );
  assert(hwnd);

  return hwnd;
}

int
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {

  HWND main_window = win32_create_window(hInstance);

  DXGI_SWAP_CHAIN_DESC swap_desc = {0};
  swap_desc.BufferDesc.RefreshRate.Numerator = 60;
  swap_desc.BufferDesc.RefreshRate.Denominator = 1;
  swap_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_desc.BufferCount = 1;
  swap_desc.OutputWindow = main_window;
  swap_desc.Windowed = true;

  ID3D11Device *device;
  ID3D11DeviceContext *device_context;
  IDXGISwapChain *swap_chain;
  D3D_FEATURE_LEVEL version;
  HRESULT init_result = D3D11CreateDeviceAndSwapChain(
    0,
    D3D_DRIVER_TYPE_HARDWARE,
    0,
    0,
    0,
    0,
    D3D11_SDK_VERSION,
    &swap_desc,
    &swap_chain,
    &device,
    &version,
    &device_context);

  assert(init_result == S_OK);

  b32 should_quit = false;
  while (!should_quit) {
    for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
      if (msg.message == WM_QUIT) {
        should_quit = true;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
}