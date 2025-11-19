/* TODO
  X -> complete
  O -> omitted

  - [X] Linux cross-compile & run with wine
  - [O] Linux multithreading
    Here's the problem. mingw-w64 gcc uses the posix thread model instead of win32.
    This emulates win32 threading API calls like CreateThread, EnterCriticalSection, etc.
    But it also implements the backend for all non win32 APIs like the std thread library.
    If we only pick one and stick with it we would be fine, easy right?
    Nope! Thread local storage is currently implemented with __declspec(thread) on MSVC, but this is
    *unsupported* on mingw-w64 gcc! So, we can't use gcc's __thread, because that's mixing the two APIs,
    and I can't find a binary of mingw-w64 gcc that uses the win32 threading model to make everything uniform.
    So the only solution now would be to rewrite all tls code to use the win32 runtime tls API. However,
    because I have already wasted half of what should've been a very productive week on this, and because this will
    probably take some time to build and debug, I have decided that Linux will just have to wait.

  - [X] Separate game & platform
  - [ ] Hot Reloading
  - [ ] Profiling (probably a codebase addition)
  - [ ] Some of `renderer_d3d11.h` is platform specific, some of it isn't, separate it!

  - [X] Deprecate vector construction functions in favor of compound literals
      and also typedef all vectors to be their construction name
      (e.g. Vec2 -> v2). This will make writing compound literals easier
      OR BETTER YET #define v2 as a macro over (Vec2) compound lit!
      I should also remove `pv2` and `dv3`
  - [X] Clean up build script (https://steve-jansen.github.io/guides/windows-batch-scripting/)
  - [X] It looks like the game is most performant with spin count = 0 for barriers?
    Verify this. Also, what is a good spin count for Critical sections?

  - [ ] Instead of a simple AABB check for determining the visible range, I should
    instead use a point-in-polygon function to support angles rotated around y-axis.
  - [ ] Make wall hight a property per room / hallway for more interesting visuals
  - [ ] For inward map corners, use the actual cornered ceiling sprite to patch the hole.
    This means that each inward corner should only be added to the list of perimeters once
    to prevent z-flimmering.
  - [ ] Audio
*/

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
#include <dxgi.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdlib.h>
#include <stdio.h>
#define ENABLE_ASSERT 1
#define DEBUG 1
#include <base/include.h>
#include <os/include.h>
#include <file/png.h>

#include "renderer_d3d11.h"
#include "dungeon.h"
#include "roguelike.h"

// NOTE: Source

#include <base/include.c>
#include <os/include.c>
#include <file/png.c>

#include "dungeon.c"
#include "roguelike.c"

#define com_release(I) if(I) IUnknown_Release(I)
//#define com_release(T)

#define R_MAX_QUADS  4096

global ID3D11Device *device;
global ID3D11DeviceContext *ctx;
global IDXGISwapChain *swap_chain;
global ID3D11RenderTargetView *render_target_view;
global ID3D11DepthStencilView *depth_stencil_view;
global ID3D11Buffer *uniforms;
global ID3D11Buffer *instance_buffer;

global Instance_Data quads[R_MAX_QUADS];
global u64 num_quads;

global b32 move_forward, move_back, strafe_left, strafe_right;

function String8
d3d11_buffer_from_blob (ID3DBlob *blob) {
  // COM makes no sense
  String8 result = {0};
  if (blob) {
    result = str8(ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob));
  }

  return result;
}

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
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

function HWND
win32_create_window (HINSTANCE hInstance) {
  WNDCLASSEX class = {
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

function Vec2i
r_init (HWND hwnd) {
  local_persist read_only R_Vertex quad_vertices[4] = {
    {{0, 0, 0}, {0, 1}},
    {{1, 0, 0}, {1, 1}},
    {{1, 1, 0}, {1, 0}},
    {{0, 1, 0}, {0, 0}},
  };
  local_persist read_only u32 quad_indices[6] = {0, 1, 2, 2, 3, 0};

  local_persist read_only D3D11_INPUT_ELEMENT_DESC input_layout_desc[] = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA,   0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0},

    {"MATRIX",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"MATRIX",   1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"MATRIX",   2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"MATRIX",   3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 80, D3D11_INPUT_PER_INSTANCE_DATA, 1},
  };

  DXGI_SWAP_CHAIN_DESC sd = {0};
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = 1;
  sd.OutputWindow = hwnd;
  sd.Windowed = 1;

  D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
  D3D11CreateDeviceAndSwapChain(0,
                                D3D_DRIVER_TYPE_HARDWARE,
                                0,
                                0,
                                &feature_level,
                                1,
                                D3D11_SDK_VERSION,
                                &sd, &swap_chain,
                                &device, 0, &ctx);

  ID3D11Texture2D *back_buffer;
  IDXGISwapChain_GetBuffer(swap_chain, 0, &IID_ID3D11Texture2D, &back_buffer);
  ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)back_buffer, 0, &render_target_view);
  D3D11_TEXTURE2D_DESC back_buffer_desc = {0};
  ID3D11Texture2D_GetDesc(back_buffer, &back_buffer_desc);
  u64 render_width = back_buffer_desc.Width;
  u64 render_height = back_buffer_desc.Height;

  // Compile & create shaders
  ID3DBlob *vs_code_blob, *ps_code_blob;
  ID3DBlob *vs_errors_blob, *ps_errors_blob;
  HRESULT vs_comp_result = D3DCompileFromFile(L""WIN32_ROGUELIKE_SOURCE_PATH"shaders.hlsl", 0, 0, "vs_main", "vs_5_0", D3DCOMPILE_DEBUG, 0, &vs_code_blob, &vs_errors_blob);
  HRESULT ps_comp_result = D3DCompileFromFile(L""WIN32_ROGUELIKE_SOURCE_PATH"shaders.hlsl", 0, 0, "ps_main", "ps_5_0", D3DCOMPILE_DEBUG, 0, &ps_code_blob, &ps_errors_blob);
  String8 vs_code, ps_code, vs_errors, ps_errors;

  if (vs_comp_result) {
    printf("D3D11: Cannot find location of vertex shader!\n");
    exit(1);
  }

  if (ps_comp_result) {
    printf("D3D11: Cannot find location of pixel shader!\n");
    exit(1);
  }

  if (vs_errors_blob) {
    vs_errors = d3d11_buffer_from_blob(vs_errors_blob);
    printf("%.*s\n", str8_expand(vs_errors));
    exit(1);
  }
  if (ps_errors_blob) {
    ps_errors = d3d11_buffer_from_blob(ps_errors_blob);
    printf("%.*s\n", str8_expand(ps_errors));
    exit(1);
  }

  vs_code = d3d11_buffer_from_blob(vs_code_blob);
  ps_code = d3d11_buffer_from_blob(ps_code_blob);

  ID3D11VertexShader *vertex_shader;
  ID3D11PixelShader *pixel_shader;
  ID3D11Device_CreateVertexShader(device, vs_code.str, vs_code.len, 0, &vertex_shader);
  ID3D11Device_CreatePixelShader(device, ps_code.str, ps_code.len, 0, &pixel_shader);
  ID3D11DeviceContext_VSSetShader(ctx, vertex_shader, 0, 0);
  ID3D11DeviceContext_PSSetShader(ctx, pixel_shader, 0, 0);

  // IA
  D3D11_BUFFER_DESC vertex_desc = {0};
  vertex_desc.ByteWidth = sizeof(R_Vertex) * array_count(quad_vertices);
  vertex_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  D3D11_SUBRESOURCE_DATA vertex_res = {0};
  vertex_res.pSysMem = quad_vertices;
  ID3D11Buffer *vbuffer;
  ID3D11Device_CreateBuffer(device, &vertex_desc, &vertex_res, &vbuffer);
  D3D11_BUFFER_DESC index_desc = {0};
  index_desc.ByteWidth = sizeof(u32) * array_count(quad_indices);
  index_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
  D3D11_SUBRESOURCE_DATA index_res = {0};
  index_res.pSysMem = quad_indices;
  ID3D11Buffer *ibuffer;
  ID3D11Device_CreateBuffer(device, &index_desc, &index_res, &ibuffer);
  D3D11_BUFFER_DESC instance_desc = {0};
  instance_desc.ByteWidth = sizeof(Instance_Data) * R_MAX_QUADS;
  instance_desc.Usage = D3D11_USAGE_DYNAMIC;
  instance_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
  instance_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  ID3D11Device_CreateBuffer(device, &instance_desc, 0, &instance_buffer);
  ID3D11InputLayout *input_layout;
  ID3D11Device_CreateInputLayout(device, input_layout_desc, array_count(input_layout_desc), vs_code.str, vs_code.len, &input_layout);
  ID3D11Buffer *vertex_buffers[] = {vbuffer, instance_buffer};
  u32 strides[] = {sizeof(R_Vertex), sizeof(Instance_Data)};
  u32 offsets[] = {0,0};
  ID3D11DeviceContext_IASetInputLayout(ctx, input_layout);
  ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, array_count(vertex_buffers), vertex_buffers, strides, offsets);
  ID3D11DeviceContext_IASetIndexBuffer(ctx, ibuffer, DXGI_FORMAT_R32_UINT, 0);
  ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  D3D11_BUFFER_DESC uniforms_desc = {0};
  uniforms_desc.ByteWidth = sizeof(Uniforms);
  uniforms_desc.Usage = D3D11_USAGE_DYNAMIC;
  uniforms_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  uniforms_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  ID3D11Device_CreateBuffer(device, &uniforms_desc, 0, &uniforms);
  ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &uniforms);

  // Rasterizer
  D3D11_VIEWPORT viewport = {0};
  viewport.Width = back_buffer_desc.Width;
  viewport.Height = back_buffer_desc.Height;
  viewport.MinDepth = 0;
  viewport.MaxDepth = 1;
  ID3D11DeviceContext_RSSetViewports(ctx, 1, &viewport);
  D3D11_RASTERIZER_DESC rstate_desc = {0};
  rstate_desc.CullMode = D3D11_CULL_NONE;
  rstate_desc.FrontCounterClockwise = 1;
  rstate_desc.DepthClipEnable = 1;
  rstate_desc.FillMode = D3D11_FILL_SOLID;
  ID3D11RasterizerState *rstate;
  ID3D11Device_CreateRasterizerState(device, &rstate_desc, &rstate);
  ID3D11DeviceContext_RSSetState(ctx, rstate);

  // OM
  D3D11_TEXTURE2D_DESC depth_texture_desc = {0};
  depth_texture_desc.Width = back_buffer_desc.Width;
  depth_texture_desc.Height = back_buffer_desc.Height;
  depth_texture_desc.MipLevels = 1;
  depth_texture_desc.ArraySize = 1;
  depth_texture_desc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
  depth_texture_desc.SampleDesc.Count = 1;
  depth_texture_desc.SampleDesc.Quality = 0;
  depth_texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  ID3D11Texture2D *depth_stencil_texture;
  ID3D11Device_CreateTexture2D(device, &depth_texture_desc, 0, &depth_stencil_texture);
  D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {0};
  depth_stencil_desc.DepthEnable = 1;
  depth_stencil_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
  depth_stencil_desc.DepthFunc = D3D11_COMPARISON_LESS;
  depth_stencil_desc.StencilEnable = 1;
  depth_stencil_desc.StencilReadMask = 0xFF;
  depth_stencil_desc.StencilWriteMask = 0xFF;
  depth_stencil_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
  depth_stencil_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
  depth_stencil_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
  depth_stencil_desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
  depth_stencil_desc.BackFace = depth_stencil_desc.FrontFace;
  ID3D11DepthStencilState *depth_stencil_state;
  ID3D11Device_CreateDepthStencilState(device, &depth_stencil_desc, &depth_stencil_state);
  ID3D11DeviceContext_OMSetDepthStencilState(ctx, depth_stencil_state, 1);
  D3D11_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc = {0};
  depth_stencil_view_desc.Format = depth_texture_desc.Format;
  depth_stencil_view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
  ID3D11Device_CreateDepthStencilView(device, (ID3D11Resource*)depth_stencil_texture, &depth_stencil_view_desc, &depth_stencil_view);
  D3D11_BLEND_DESC blend_desc = {0};
  blend_desc.RenderTarget[0].BlendEnable = 1;
  blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
  blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
  blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
  blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
  blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
  ID3D11BlendState *blend_state;
  ID3D11Device_CreateBlendState(device, &blend_desc, &blend_state);
  ID3D11DeviceContext_OMSetBlendState(ctx, blend_state, 0, 0xFFFFFFFF);
  ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &render_target_view, depth_stencil_view);

  com_release(back_buffer);
  com_release(vs_code_blob);
  com_release(ps_code_blob);
  com_release(vs_errors_blob);
  com_release(ps_errors_blob);

  return v2i(render_width, render_height);
}

function void
r_create_and_bind_texture (PNG_Bitmap_RGBA raw_texture_data, b32 generate_mipmaps) {
  D3D11_TEXTURE2D_DESC tex_desc = {0};
  tex_desc.Width = raw_texture_data.width;
  tex_desc.Height = raw_texture_data.height;
  tex_desc.MipLevels = !generate_mipmaps;
  tex_desc.ArraySize = 1;
  tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  tex_desc.SampleDesc.Count = 1;
  tex_desc.SampleDesc.Quality = 0;
  tex_desc.Usage = D3D11_USAGE_DEFAULT;
  tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  tex_desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

  D3D11_SHADER_RESOURCE_VIEW_DESC tex_srv = {0};
  tex_srv.Format = tex_desc.Format;
  tex_srv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
  tex_srv.Texture2D.MipLevels = -1;
  tex_srv.Texture2D.MostDetailedMip = 0;

  ID3D11Texture2D *tex;
  ID3D11ShaderResourceView *tex_view;
  ID3D11Device_CreateTexture2D(device, &tex_desc, NULL, &tex);
  ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource*)tex, 0, NULL, raw_texture_data.pixels, raw_texture_data.width * sizeof(u32), 0);

  ID3D11Device_CreateShaderResourceView(device, (ID3D11Resource*)tex, &tex_srv, &tex_view);
  ID3D11DeviceContext_GenerateMips(ctx, tex_view);
  ID3D11DeviceContext_VSSetShaderResources(ctx, 0, 1, &tex_view);
  ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &tex_view);

  D3D11_SAMPLER_DESC sampler_desc;
  sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  sampler_desc.MinLOD = 0;
  sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
  sampler_desc.MipLODBias = 0.f;
  sampler_desc.MaxAnisotropy = 16;
  sampler_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
  sampler_desc.BorderColor[0] = 1.f;
  sampler_desc.BorderColor[1] = 1.f;
  sampler_desc.BorderColor[2] = 1.f;
  sampler_desc.BorderColor[3] = 1.f;
  ID3D11SamplerState *sampler;
  ID3D11Device_CreateSamplerState(device, &sampler_desc, &sampler);
  ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &sampler);
}

function void
r_prep (void) {
  if (runner_id() == 0) {
    local_persist read_only f32 clear_color[4] = {0.1f, 0.2f, 0.3f, 1.f};
    memory_zero(quads, num_quads);
    num_quads = 0;
    ID3D11DeviceContext_ClearDepthStencilView(ctx, depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
    ID3D11DeviceContext_ClearRenderTargetView(ctx, render_target_view, clear_color);
  }
}

function void
r_update_transform (Mat4 m) {
  if (runner_id() == 0) {
    D3D11_MAPPED_SUBRESOURCE constant_buffer = {0};
    Uniforms new_transform_data = {m};
    ID3D11DeviceContext_Map(ctx, (ID3D11Resource*)uniforms, 0, D3D11_MAP_WRITE_DISCARD, 0, &constant_buffer);
    memcpy(constant_buffer.pData, &new_transform_data, sizeof(Uniforms));
    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource*)uniforms, 0);
  }
}

function void
r_push_quad_ (Push_Quad_Params *p) {
  Instance_Data *next_inst;
  next_inst = &quads[InterlockedIncrement64(&num_quads)-1];

  Vec2 scale = p->scale;
  Atlas_Coords coords = p->atlas_coords;
  if (p->sprite.name.len) {
    coords = p->sprite.coords[0];
    scale = coords.scale;
  }
  Mat4 T = m4translate(p->pos);
  Mat4 R = m4rotate(p->rot);
  R = m4mul(m4translate(v3(.xy=p->rot_offset)), R);
  R = m4mul(R, m4translate(v3(-p->rot_offset.x, -p->rot_offset.y, 0)));
  Mat4 S = m4scale(v3(.xy=scale));
  Mat4 world = m4mul(T,R);
  world = m4mul(world,S);

  next_inst->world = world;
  next_inst->atlas_coords = coords;
  next_inst->color = p->col;
}

function void
r_present (b32 enable_vsync) {
  if (runner_id() == 0) {
    D3D11_MAPPED_SUBRESOURCE instances = {0};

    ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &render_target_view, depth_stencil_view);
    ID3D11DeviceContext_Map(ctx, (ID3D11Resource*)instance_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &instances);
    memcpy(instances.pData, quads, sizeof(Instance_Data) * num_quads);
    ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource*)instance_buffer, 0);
    ID3D11DeviceContext_DrawIndexedInstanced(ctx, 6, num_quads, 0, 0, 0);

    IDXGISwapChain_Present(swap_chain, enable_vsync, 0);
  }
}

function void
r_draw_entity (Entity *e) {
  Sprite *anim = &e->run;
  Sprite *prev_anim = &e->idle;
  if (e->dir == 0) {
    swap(anim, prev_anim);
  }
  if (prev_anim->started_at || anim->started_at == 0) {
    anim->started_at = os_clock_seconds();
    anim->current_frame = 0;

    prev_anim->started_at = 0;
  }

  f32 seconds_per_frame = anim->seconds_to_complete / anim->num_frames;
  f32 current_step = anim->started_at + seconds_per_frame * anim->current_frame;
  f32 now = os_clock_seconds();
  if (now - current_step >= seconds_per_frame) {
    anim->current_frame++;
    if (anim->current_frame == anim->num_frames) {
      anim->started_at += seconds_per_frame * anim->current_frame;
    }
    anim->current_frame %= anim->num_frames;
  }

  Quat rot = axis_angle(v3(0,1,0), e->rotation_angle);
  Atlas_Coords texcoord = anim->coords[anim->current_frame];
  r_push_quad(.pos = e->pos, .scale = texcoord.scale, .rot = rot, .rot_offset = v2(texcoord.scale.x/2.f, 0), .atlas_coords = texcoord);
}

void
os_entry (void) {
  // NOTE: Initialization, some aspects, like dungeon creation/partitioning can potentially be parallelized if
  // they become performance problems.
  void *gs;
  Arena *perm;
  Arena *frame;
  if (runner_id() == 0) {
    perm = arena_alloc();
    frame = arena_alloc();
    srand(os_query_clock());

    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND hwnd = win32_create_window(hInstance);
    Vec2i render_dim = r_init(hwnd);
    f32 render_width = render_dim.width;
    f32 render_height = render_dim.height;

    gs = roguelike_init((Game_Init_Package){
      perm, frame,
      str8_lit(WIN32_ROGUELIKE_SOURCE_PATH),
      str8_lit(WIN32_ROGUELIKE_ASSET_PATH),
      render_width, render_height
      });
  }
  os_heat_sync_u64((u64*)&gs, 0);
  os_heat_sync_u64((u64*)&perm, 0);
  os_heat_sync_u64((u64*)&frame, 0);

  u64 last = os_query_clock(), now = 0;
  f32 dt = 0;
  for (;;) {
    if (runner_id() == 0) { // TODO: Can we parallelize *anything* in the message loop?
      arena_clear(frame);

      // Input
      for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
        if (msg.message == WM_QUIT) {
          ExitProcess(0);
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      // Update TODO: Parallelize
      now = os_query_clock();
      dt = os_get_elapsed_ms(last, now);
      last = now;

      roguelike_tick(gs, dt, (Game_Input_Package){move_forward, move_back, strafe_left, strafe_right});
    }
    os_heat_sync();

    // Render
    roguelike_draw(gs);
  }
}