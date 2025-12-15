// TODO: In the future everything will be bundled with the exe.
#ifndef WIN32_ROGUELIKE_SOURCE_PATH
# error "WIN32_ROGUELIKE_SOURCE_PATH is undefined"
#endif

#ifndef WIN32_ROGUELIKE_ASSET_PATH
# error "WIN32_ROGUELIKE_ASSET_PATH is undefined"
#endif

// NOTE: Headers

//#define UNICODE
#define D3D11_NO_HELPERS
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <stdio.h>
#define ENABLE_ASSERT 1
#define DEBUG 1
#include <base/include.h>
#include <os/include.h>
#include <file/png.h>

#include "graphics.h"
#include "dungeon.h"
#include "roguelike.h"

// NOTE: Source

#include <base/include.c>
#include <os/include.c>
#include <file/png.c>
#if GRAPHICS_FORCE_OPENGL
# include "graphics_ogl.c"
#else
# include "graphics_d3d11.c"
#endif

global b32 move_forward, move_back, strafe_left, strafe_right, mouse_click;
global f32 mouse_x, mouse_y;
global Vec2i render_dim;
global Game_Input_Package old_input, new_input;

function LRESULT
WndProc (HWND hwnd, u32 uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_KEYUP: fallthrough
    case WM_KEYDOWN: {
      b32 key_down = !(lParam >> 31);
      if (wParam == 'W') {
        move_forward = key_down;
      }
      if (wParam == 'A') {
        strafe_left = key_down;
      }
      if (wParam == 'S') {
        move_back = key_down;
      }
      if (wParam == 'D') {
        strafe_right = key_down;
      }
      return 0;
    }

    case WM_LBUTTONUP:   fallthrough
    case WM_LBUTTONDOWN: fallthrough
    case WM_MOUSEMOVE: {
      mouse_click = (wParam & 0x0001);
      mouse_x = (f32)GET_X_LPARAM(lParam);
      mouse_y = (f32)GET_Y_LPARAM(lParam);
      return 0;
    }
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

function HWND
win32_create_window (HINSTANCE hInstance) {
  WNDCLASSEX class = {
    .style = CS_OWNDC,
    .cbSize = sizeof(WNDCLASSEX),
    .lpfnWndProc = WndProc,
    .hInstance = hInstance,
    .hCursor = LoadCursor(0, IDC_ARROW),
    .lpszClassName = TEXT("MainWindowClass"),
  };
  RegisterClassEx(&class);

  HWND hwnd = CreateWindowEx(
    0,
    TEXT("MainWindowClass"),
    TEXT("Halfway Heroes"),
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    0,
    0,
    hInstance,
    0);

  return hwnd;
}

function void
win32_update_game_dll (HMODULE *game_code, Game_VTable *game_vtable) {
  if (PathFileExists(TEXT("roguelike_new.dll"))) {
    if (*game_code) {
      FreeLibrary(*game_code);
      *game_code = NULL;
    }
    while (!MoveFileEx(TEXT("roguelike_new.dll"), TEXT("roguelike.dll"), MOVEFILE_REPLACE_EXISTING));
  }

  if (*game_code == NULL) {
    *game_code = LoadLibrary(TEXT("roguelike.dll"));
    assert (*game_code);

    game_vtable->init = (roguelike_init_type)GetProcAddress(*game_code, "roguelike_init");
    game_vtable->tick = (roguelike_tick_type)GetProcAddress(*game_code, "roguelike_tick");
    game_vtable->draw = (roguelike_draw_type)GetProcAddress(*game_code, "roguelike_draw");
    if (game_vtable->init == NULL) {
      printf("Unable to load game code!\n");
      assert(0);
    }
  }
}

void
os_entry (void) {
  // NOTE: Initialization, some aspects, like dungeon creation/partitioning can potentially be parallelized if
  // they become performance problems.
  void *gs = 0;
  Game_VTable *game = 0;
  HMODULE game_dll = 0;
  Arena *perm = 0;
  Arena *frame = 0;
  if (runner_id() == 0) {
    perm = arena_alloc();
    frame = arena_alloc();

    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND hwnd = win32_create_window(hInstance);
    render_dim = r_init(hwnd);
    f32 render_width = render_dim.width;
    f32 render_height = render_dim.height;

    game = arena_pushn(perm, Game_VTable, 1);
    win32_update_game_dll(&game_dll, game);

    gs = game->init(os_get_thread_context(),(Game_Init_Package){
      perm, frame,
      str8_lit(WIN32_ROGUELIKE_SOURCE_PATH),
      str8_lit(WIN32_ROGUELIKE_ASSET_PATH),
      render_width, render_height,
      (Renderer_VTable){r_create_texture, r_bind_texture, r_prep, r_update_transform, r_push_quad_, r_draw_quads, r_present}
      });

  }
  os_heat_sync_ptr(gs, 0);
  os_heat_sync_ptr(game_dll, 0);
  os_heat_sync_ptr(game, 0);
  os_heat_sync_ptr(perm, 0);
  os_heat_sync_ptr(frame, 0);

  u64 last = os_query_clock(), now = 0;
  f32 dt = 0;
  for (;;) {
    if (runner_id() == 0) { // TODO: Can we parallelize *anything* in the message loop?
      arena_clear(frame);
      old_input = new_input;

      // Input
      for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
        if (msg.message == WM_QUIT) {
          ExitProcess(0);
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      win32_update_game_dll(&game_dll, game);

      now = os_query_clock();
      dt = os_get_elapsed_ms(last, now);
      last = now;
    }
    os_heat_sync_ptr(dt, 0);
    new_input.move_forward = move_forward;
    new_input.move_back    = move_back;
    new_input.strafe_left  = strafe_left;
    new_input.strafe_right = strafe_right;
    new_input.action_primary = mouse_click;
    new_input.cursor = v2(mouse_x, render_dim.height - mouse_y);
    game->tick(os_get_thread_context(), gs, dt, old_input, new_input);
    os_heat_sync();

    // Render
    game->draw(os_get_thread_context(), gs);
  }
}