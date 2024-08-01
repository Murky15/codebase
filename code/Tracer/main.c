#include <Windows.h>
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")

#include "base/include.h"

#include "base/include.c"
#include "renderer.c"

#define CANVAS_WIDTH 1280
#define CANVAS_HEIGHT 720

#define DebugPrint(str8) OutputDebugString((const char*)str8.str)

typedef struct Win32_Data {
  HINSTANCE hInstance;
  HWND hwnd;
  HDC win_dc;
  BITMAPINFO bitmap;
} Win32_Data;

global b32 game_running = true;

function LRESULT
Wndproc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_CLOSE: PostQuitMessage(0); return 0; break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

function Win32_Data
win32_create_window (HINSTANCE hInstance) {
  WNDCLASS wc = {0};
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = Wndproc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = GetStockObject(WHITE_BRUSH);
  wc.lpszClassName = "tracer_main_window_class";
  RegisterClass(&wc);

  RECT dim = {0, 0, CANVAS_WIDTH, CANVAS_HEIGHT};
  AdjustWindowRect(&dim, WS_OVERLAPPEDWINDOW, 0);
  HWND hwnd = CreateWindow(
    wc.lpszClassName,
    "Tracer: DEV BUILD",
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

  return result;
}

function BITMAPINFO
win32_create_bitmap(u32 width, u32 height) {
  BITMAPINFOHEADER header = {0};
  header.biSize = sizeof(header);
  header.biWidth = CANVAS_WIDTH;
  header.biHeight = -CANVAS_HEIGHT;
  header.biPlanes = 1;
  header.biBitCount = 32;
  header.biCompression = BI_RGB;
  return (BITMAPINFO){header};
}

function void
render_silly_little_thing (u32 *pixels, u32 width, u32 height) {
  local_persist u8 xOffset, yOffset;
  for (u32 y = 0; y < height; ++y) {
    for (u32 x = 0; x < width; ++x) {
      u8 r = (u8)x + xOffset;
      u8 g = (u8)y + yOffset;
      u8 b = 0;
      pixels[y * width + x] = (r << 16 | g << 8 | b);
    }
  }

  // I'm dizzy
  // xOffset = ++yOffset;
}

function int
WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
  Arena *perm_arena = arena_alloc();
  Arena *frame_arena = arena_alloc();

  Win32_Data platform = win32_create_window(hInstance);

  // @note: Register for input
  RAWINPUTDEVICE input_devices[1];

  // Keyboard
  input_devices[0].usUsagePage = 0x01;
  input_devices[0].usUsage = 0x06;
  input_devices[0].dwFlags = RIDEV_NOLEGACY;
  input_devices[0].hwndTarget = 0;

#if 0
  // Mouse
  input_devices[1].usUsagePage = 0x01;
  input_devices[1].usUsage = 0x02;
  input_devices[1].dwFlags = 0; // For some reason RIDEV_NOLEGACY hangs program?
  input_devices[1].hwndTarget = 0;
#endif

  if (RegisterRawInputDevices(input_devices, array_count(input_devices), sizeof(input_devices[0])) == FALSE) {
    OutputDebugString("Unable to register input devices\n");
  }

  // @note: Create bitmap
  void *pixels = arena_pushn(perm_arena, u32, CANVAS_WIDTH * CANVAS_HEIGHT);
  platform.bitmap = win32_create_bitmap(CANVAS_WIDTH, CANVAS_HEIGHT);
  Bitmap canvas = {pixels, platform.bitmap.bmiHeader.biWidth, platform.bitmap.bmiHeader.biHeight, platform.bitmap.bmiHeader.biBitCount / 8};

  for (;game_running;) {
    // @note: Message loop
    for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
      if (msg.message == WM_QUIT) {
        game_running = false;
      }

      // @todo: Maybe move this to WndProc?
      if (msg.message == WM_INPUT) {
        UINT size;
        local_persist u8 data[sizeof(RAWINPUT)];
        GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER));
        RAWINPUT *input = (RAWINPUT*)data;

        if (input->header.dwType == RIM_TYPEKEYBOARD) {
          RAWKEYBOARD *keyboard = &input->data.keyboard;
          b32 key_down = keyboard->Flags & RI_KEY_MAKE;
          if (keyboard->MakeCode != KEYBOARD_OVERRUN_MAKE_CODE) {
            u8 extension = keyboard->Flags & RI_KEY_E0 ? 0xE0 : keyboard->Flags & RI_KEY_E1 ? 0xE1 : 0x00;
            u16 scan_code = (extension << 8) | (keyboard->MakeCode & 0x7F);

            switch (scan_code) {
              case 0x0011: fallthrough
              case 0xE048: {

              } break;
            }

            switch (scan_code) {
              case 0x001F: fallthrough
              case 0xE050: {

              } break;
            }

            switch (scan_code) {
              case 0x001E: fallthrough
              case 0xE04B: {

              } break;
            }

            switch (scan_code) {
              case 0x0020: fallthrough
              case 0xE04D: {

              } break;
            }
          }
        }
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // @note: Update

    // @note: Render
    render_silly_little_thing((u32*)pixels, CANVAS_WIDTH, CANVAS_HEIGHT);

    StretchDIBits(
      platform.win_dc,
      0, 0,
      CANVAS_WIDTH,
      CANVAS_HEIGHT,
      0, 0,
      CANVAS_WIDTH,
      CANVAS_HEIGHT,
      pixels,
      &platform.bitmap,
      DIB_RGB_COLORS,
      SRCCOPY
    );

    arena_clear(frame_arena);
  }

  return 0;
}