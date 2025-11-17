/* TODO
  - [ ] Linux compile & run with wine
  - [ ] Hot Reloading (seperate game & platform)
  - [ ] Profiling (probably a codebase addition)
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

// NOTE: Headers

//#define UNICODE
#define D3D11_NO_HELPERS
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <dxgi.h>
#include <d3d11.h>
#include <D3DCompiler.h>
#include <stdlib.h>
#include <stdio.h>
#define ENABLE_ASSERT 1
#define DEBUG 1
#include <base/include.h>
#include <os/include.h>
#include <file/png.h>

#include "roguelike.h"
#include "dungeon.h"

// NOTE: Source

#include <base/include.c>
#include <os/include.c>
#include <file/png.c>

#include "dungeon.c"

#define com_release(I) if(I) IUnknown_Release(I)
//#define com_release(T)

#define R_MAX_QUADS  4096
#define PLAYER_MOVE_SPEED 0.15f

typedef struct Game_State {
  Arena *perm, *frame;
  Texture_Atlas sprites;
  Dungeon dungeon;
  Mat4 proj;
  Entity player;
  Camera cam;
} Game_State;

typedef struct R_Vertex {
  Vec3 pos;
  Vec2 uv;
} R_Vertex;

typedef struct Uniforms {
  Mat4 view_proj;
} Uniforms;

typedef struct Instance_Data {
  Mat4 world;
  Atlas_Coords atlas_coords;
  Vec4 color;
} Instance_Data;

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

function u64
win32_get_perf_frequency (void) {
  local_persist threadvar LARGE_INTEGER freq;
  if (freq.QuadPart == 0)
    QueryPerformanceFrequency(&freq);

  return freq.QuadPart;
}

function u64
win32_query_clock (void) {
  LARGE_INTEGER tick;
  QueryPerformanceCounter(&tick);

  return tick.QuadPart;
}

function f32
win32_get_elapsed_ms (u64 t1, u64 t2) {
  u64 freq = win32_get_perf_frequency();
  u64 elapsed_ms = (t2 - t1) * 1000;

  return (f32)elapsed_ms / freq;
}

function f64
win32_clock_seconds (void) {
  u64 freq = win32_get_perf_frequency();
  u64 current_time = win32_query_clock();

  return (f64)current_time / freq;
}

function u64
win32_ms_to_tick_interval (f32 ms) {
  u64 freq = win32_get_perf_frequency();
  u64 ticks = ms * freq;
  ticks /= 1000;

  return ticks;
}

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
  D3DCompileFromFile(L"W:/code/roguelike/shaders.hlsl", 0, 0, "vs_main", "vs_5_0", D3DCOMPILE_DEBUG, 0, &vs_code_blob, &vs_errors_blob);
  D3DCompileFromFile(L"W:/code/roguelike/shaders.hlsl", 0, 0, "ps_main", "ps_5_0", D3DCOMPILE_DEBUG, 0, &ps_code_blob, &ps_errors_blob);
  String8 vs_code, ps_code, vs_errors, ps_errors;
  vs_code = d3d11_buffer_from_blob(vs_code_blob);
  ps_code = d3d11_buffer_from_blob(ps_code_blob);
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
  ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, array_count(vertex_buffers), vertex_buffers, strides, offsets);
  ID3D11DeviceContext_IASetIndexBuffer(ctx, ibuffer, DXGI_FORMAT_R32_UINT, 0);
  ID3D11DeviceContext_IASetInputLayout(ctx, input_layout);
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

// We might be able to auto-generate this through metaprogramming
typedef struct Push_Quad_Params {
  Vec3 pos;
  Vec2 scale;
  Vec2 rot_offset;
  Vec4 col;
  Quat rot;
  Atlas_Coords atlas_coords;
  Sprite sprite;
} Push_Quad_Params;
#define r_push_quad(...) r_push_quad_(&(Push_Quad_Params){ \
  .scale = v2(1,1), \
  .col = v4(1,1,1,1), \
  .rot = qi(), \
  __VA_ARGS__ \
  }, false)

function void
r_push_quad_ (Push_Quad_Params *p, b32 is_decal) {
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

function Cardinal_Dir
to_cardinal (Vec2 dir) {
  Cardinal_Dir result = 0;
  if (dir.x != 0) {
    result |= dir.x > 0 ? EAST : WEST;
  }
  if (dir.y != 0) {
    result |= dir.y > 0 ? NORTH : SOUTH;
  }

  return result;
}

// These could probably be named better
function Sprite*
get_atlas_slot (Texture_Atlas atlas, String8 key) {
  Sprite *result = 0;
  u64 hash = str8_hash(key) % atlas.num_sprites;
  while (true) {
    Sprite *selected = &atlas.sprites[hash];
    if (str8_match(selected->name, key, 0) || selected->name.len == 0) {
      result = selected;
      break;
    }
    hash = (hash + 1) % atlas.num_sprites;
  }

  return result;
}

function Sprite
get_sprite (Texture_Atlas atlas, String8 key) {
  return *get_atlas_slot(atlas, key);
}

function Atlas_Coords
make_atlas_coords_from_string (String8 coords) {
  Atlas_Coords result = {0};
  Temp_Arena scratch;
  ldefer (scratch=get_scratch(0,0), release_scratch(scratch)) {
    String8List numbers = str8_split(scratch.arena, coords, 1, " ");
    Vec4 coords = {0};
    u64 i = 0;
    for each_in_list (num, &numbers) {
      coords.e[i++] = f64_from_str8(num->string);
    }
    result.scale = coords.zw;
    result.offset = coords.xy;
  }

  return result;
}

function Texture_Atlas
load_textures (Arena *arena, String8 absolute_path_to_asset_dir) {
  Texture_Atlas result = {0};
  Temp_Arena scratch;
  ldefer(scratch=get_scratch(&arena, 1),release_scratch(scratch)) {
    String8 path_to_atlas_data = str8_pushf(scratch.arena,
      "%.*s/tile_list_v1.7", str8_expand(absolute_path_to_asset_dir));
    String8 atlas_txt = os_read_file(arena, path_to_atlas_data, false);

    String8List atlas_lines = str8_split(scratch.arena, atlas_txt, 1, "\n");
    result.num_sprites = atlas_lines.num_nodes;
    result.sprites = arena_pushn(arena, Sprite, result.num_sprites);
    for each_in_list(line, &atlas_lines) {
      u64 sep_pos = str8_find(line->string, str8_lit(" "), 0, 0);
      String8 name = str8_prefix(line->string, sep_pos);
      String8 coords = str8_skip(line->string, sep_pos+1);
      String8 frame = str8_sub(name, name.len-3, name.len-1);

      if (str8_match(frame, str8_lit("_f"), 0)) {
        name = str8_chop(name, 3);
      }
      Sprite *sprite = get_atlas_slot(result, name);
      if (sprite->name.len == 0) {
        sprite->name = name;
      }
      sprite->coords[sprite->num_frames++] = make_atlas_coords_from_string(coords);
    }

    String8 path_to_texture_data = str8_pushf(scratch.arena,
      "%.*s/0x72_DungeonTilesetII_v1.7.png", str8_expand(absolute_path_to_asset_dir));
    String8 texture_png_data = os_read_file(scratch.arena, path_to_texture_data, false);
    result.raw_texture_data = png_decode(arena, texture_png_data);
  }
  return result;
}

function void
r_draw_entity (Entity *e) {
  Sprite *anim = &e->run;
  Sprite *prev_anim = &e->idle;
  if (e->dir == 0) {
    swap(anim, prev_anim);
  }
  if (prev_anim->started_at || anim->started_at == 0) {
    anim->started_at = win32_clock_seconds();
    anim->current_frame = 0;

    prev_anim->started_at = 0;
  }

  f32 seconds_per_frame = anim->seconds_to_complete / anim->num_frames;
  f32 current_step = anim->started_at + seconds_per_frame * anim->current_frame;
  f32 now = win32_clock_seconds();
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

function Rect
cam_calculate_visible_range (Camera cam, f32 fov_h, f32 aspect_ratio, f32 znear) {
  // Calculate corners of the near plane
  f32 near_height = 2.f * tanf(fov_h*0.5f) * znear;
  f32 near_width = near_height * aspect_ratio;

  // Man I wish we had operator overloading...
  Vec3 camera_dir = v3norm(v3sub(cam.focus, cam.pos));
  Vec3 camera_right = v3norm(v3cross(v3(0,1,0), camera_dir));
  Vec3 camera_up = v3norm(v3cross(camera_dir, camera_right));

  Vec2 half_near = v2muls(v2(near_width, near_height), 0.5f);
  Vec3 near_center = v3add(cam.pos, v3muls(camera_dir, znear));

  Vec3 ntl = v3sub(v3add(near_center, v3muls(camera_up, half_near.height)), v3muls(camera_right, half_near.width));
  Vec3 ntr = v3add(v3add(near_center, v3muls(camera_up, half_near.height)), v3muls(camera_right, half_near.width));
  Vec3 nbl = v3sub(v3sub(near_center, v3muls(camera_up, half_near.height)), v3muls(camera_right, half_near.width));
  Vec3 nbr = v3add(v3sub(near_center, v3muls(camera_up, half_near.height)), v3muls(camera_right, half_near.width));

  Vec3 line_top_left = v3sub(ntl, cam.pos);
  Vec3 line_top_right = v3sub(ntr, cam.pos);
  Vec3 line_bottom_left = v3sub(nbl, cam.pos);
  Vec3 line_bottom_right = v3sub(nbr, cam.pos);

  f32 t0 = -cam.pos.y / line_top_left.y;
  f32 t1 = -cam.pos.y / line_top_right.y;
  f32 t2 = -cam.pos.y / line_bottom_left.y;
  f32 t3 = -cam.pos.y / line_bottom_right.y;

  Vec2 top_left = v2(cam.pos.x + line_top_left.x * t0, cam.pos.z + line_top_left.z * t0);
  Vec2 top_right = v2(cam.pos.x + line_top_right.x * t1, cam.pos.z + line_top_right.z * t1);
  Vec2 bottom_left = v2(cam.pos.x + line_bottom_left.x * t2, cam.pos.z + line_bottom_left.z * t2);
  Vec2 bottom_right = v2(cam.pos.x + line_bottom_right.x * t3, cam.pos.z + line_bottom_right.z * t3);

  f32 min_x = min(top_left.x, bottom_left.x);
  f32 max_x = max(top_right.x, bottom_right.x);
  f32 x_diff = max_x - min_x;
  f32 y_diff = top_left.y - bottom_left.y;

  return (Rect){.xy=v2(min_x, bottom_left.y), .zw=v2(x_diff, y_diff)};
}

void
os_entry (void) {
  // NOTE: Initialization, some aspects, like dungeon creation/partitioning can potentially be parallelized if
  // they become performance problems.
  Game_State *gs;

  if (runner_id() == 0) {
    Arena *perm = arena_alloc();
    Arena *frame = arena_alloc();
    srand(win32_query_clock());

    gs = arena_pushn(perm, Game_State, 1);
    gs->perm = perm;
    gs->frame = frame;

    HINSTANCE hInstance = GetModuleHandle(NULL);
    HWND hwnd = win32_create_window(hInstance);
    Vec2i render_dim = r_init(hwnd);
    f32 render_width = render_dim.width;
    f32 render_height = render_dim.height;

    Texture_Atlas sprites = load_textures(perm, str8_lit("W:/assets/roguelike/0x72_DungeonTilesetII_v1.7"));
    r_create_and_bind_texture(sprites.raw_texture_data, true);

    Dungeon dungeon = d_create(perm, sprites,
      .target_room_count = 500,
      .grid_dim   = 16,
      .map_width  = 512,
      .map_height = 512,
      .room_width_mean = 15,
      .room_width_deviation = 5,
      .room_height_mean = 15,
      .room_height_deviation = 5,
      .hallway_width = 3,
      .percent_edges_included = 12,
      .percent_tiles_cracked = 5);

    Entity player = {0};
    player.pos = v3(0,1,0);
    player.seconds_to_rotate = 0.12f;
    player.idle = get_sprite(sprites, str8_lit("doc_idle_anim"));
    player.idle.seconds_to_complete = 0.5f;
    player.run  = get_sprite(sprites, str8_lit("doc_run_anim"));
    player.run.seconds_to_complete = 0.5f;

    f32 fov_h = M_PI32/4.f;
    f32 aspect_ratio = render_width/render_height;
    f32 znear = 20.f;
    f32 zfar = 500.f;

    Mat4 proj = m4perspective(fov_h, aspect_ratio, znear, zfar);
    Camera cam = {0};
    f32 cam_zoom = 150.f;
    //cam.pos = v3(-cam_zoom/sqrtf(2.f), cam_zoom*sinf(atanf(1.f/sqrtf(2.f))), -cam_zoom/sqrtf(2.f));
    //cam.pos = v3(0, -cam_zoom, 1);
    cam.pos = v3(player.idle.coords[0].scale.x/2.f, cam_zoom, -cam_zoom);
    cam.focus = v3(cam.pos.x, player.pos.y, 0);
    cam.follow_dist = v3sub(cam.pos,cam.focus);
    cam.visible_range = cam_calculate_visible_range(cam, fov_h, aspect_ratio, znear);

    gs->sprites = sprites;
    gs->dungeon = dungeon;
    gs->proj = proj;
    gs->player = player;
    gs->cam = cam;
  }

  os_heat_sync_u64((u64*)&gs, 0);

  Quat floor_rot = axis_angle(v3(1,0,0), M_PI32/2.f);
  Quat forward_wall_rot = axis_angle(v3(0,1,0), M_PI32/2.f);

  // NOTE: This sprite pack comes with a variety of sprites, but we can only use a few of them because of the 3D perspective
  Sprite spr_wall_mid = get_sprite(gs->sprites, str8_lit("wall_mid"));
  Sprite spr_ceil = get_sprite(gs->sprites, str8_lit("wall_top_mid"));
  Vec4 ceil_color = v4(0.13f,0.13f,0.13f,1);


  u64 last = win32_query_clock(), now = 0;
  f32 dt = 0;
  for (;;) {
    if (runner_id() == 0) { // TODO: Can we parallelize *anything* in the message loop?
      arena_clear(gs->frame);

      // Input
      for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
        if (msg.message == WM_QUIT) {
          ExitProcess(0);
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      // Update TODO: Parallelize
      now = win32_query_clock();
      dt = win32_get_elapsed_ms(last, now);
      last = now;

      //Quat cam_rot = axis_angle(v3(0,1,0), fmod_cycling(win32_clock_seconds(), 2 * M_PI))
      //cam_pos = m4mulv(m4rotate(cam_rot), cam_pos);
      Vec2 move_dir = {0};
      if (move_forward) move_dir.y += 1;
      if (strafe_left)  move_dir.x -= 1;
      if (move_back)    move_dir.y -= 1;
      if (strafe_right) move_dir.x += 1;
      if (v2len(move_dir) > 1) move_dir = v2norm(move_dir);
      gs->cam.pos = v3add(gs->cam.pos, v3muls(v3(move_dir.x, 0, move_dir.y), dt * PLAYER_MOVE_SPEED));
      gs->cam.focus = v3add(gs->cam.focus, v3muls(v3(move_dir.x, 0, move_dir.y), dt * PLAYER_MOVE_SPEED));
      gs->cam.visible_range.xy = v2add(gs->cam.visible_range.xy, v2muls(move_dir, dt * PLAYER_MOVE_SPEED));

      gs->player.pos = v3add(gs->player.pos, v3muls(v3(move_dir.x, 0, move_dir.y), dt * PLAYER_MOVE_SPEED));

      gs->player.dir = to_cardinal(move_dir);
      if (gs->player.dir & EAST) {
        gs->player.end_angle = 0;
      } else if (gs->player.dir & WEST) {
        gs->player.end_angle = M_PI32;
      }

      if (!almost_equal(gs->player.rotation_angle, gs->player.end_angle)) {
        if (!gs->player.started_rotating_at) {
          gs->player.started_rotating_at = win32_clock_seconds();
        }
        f64 current_time = win32_clock_seconds();
        f64 rot_amt = cnorm(current_time, gs->player.started_rotating_at, gs->player.started_rotating_at + gs->player.seconds_to_rotate);
        gs->player.rotation_angle = lerp(gs->player.start_angle, gs->player.end_angle, rot_amt);
      } else {
        gs->player.start_angle = gs->player.end_angle;
        gs->player.started_rotating_at = 0;
      }
    }
    os_heat_sync();

    r_prep();

    Mat4 view = m4lookat(gs->cam.pos, gs->cam.focus, v3(0,1,0));
    Mat4 VP = m4mul(gs->proj, view);

    Rect player_visible_range;
    player_visible_range.xy = d_world_to_grid(&gs->dungeon, gs->cam.visible_range.xy);
    player_visible_range.zw = d_world_to_grid(&gs->dungeon, gs->cam.visible_range.zw);
    // Apply buffer
    f32 buff_amt_tiles = 3;
    player_visible_range.xy = v2sub(player_visible_range.xy, v2(buff_amt_tiles,buff_amt_tiles));
    player_visible_range.zw = v2add(player_visible_range.zw, v2(buff_amt_tiles*2,buff_amt_tiles*2));
    Dungeon_Tile_List visible_tile_list = d_query_range(gs->frame, gs->dungeon.map, player_visible_range, true);

    Dungeon_Tile *visible_tiles;
    Dungeon_Perimeter_Tile *perimeter;
    if (runner_id() == 0) {
      visible_tiles = arena_pushn(gs->frame, Dungeon_Tile, visible_tile_list.count);
      perimeter = arena_pushn(gs->frame, Dungeon_Perimeter_Tile, visible_tile_list.num_perimeter);

      u64 pidx = 0;
      for each_in_list (tile_node, &visible_tile_list) {
        u64 i = (tile_node - visible_tile_list.first);
        Dungeon_Tile tile = tile_node->tile;
        visible_tiles[i] = tile;

        for (u64 p = 0; p < tile.on_perimeter; ++p) {
          Dungeon_Perimeter_Tile *perim = &perimeter[pidx++];
          perim->sprite = spr_wall_mid;
          perim->grid_pos = v2add(tile.grid_pos, tile.perim[p].offset);
          perim->lateral = tile.perim[p].lateral;
          perim->requires_ceil_adjustment = !tile.perim[p].side;
        }
      }
    }
    os_heat_sync_u64((u64*)&visible_tiles, 0);
    os_heat_sync_u64((u64*)&perimeter, 0);

    u64 wall_height = 3;
    f32 ceil_height = wall_height * gs->dungeon.grid_dim;

    Rangei visible_snippet = os_heat_distribute(visible_tile_list.count);
    for each_in_range (tile, visible_tiles, visible_snippet) {
      Vec2 world = d_grid_to_world(&gs->dungeon, tile->grid_pos);
      Vec3 pos = v3(world.x, 1, world.y);
      Sprite sprite = tile->sprite;
      if (tile->flags == DUNGEON_TILE_EMPTY) {
        pos = v3(pos.x, ceil_height, pos.z);
        Vec2 scale = v2(gs->dungeon.grid_dim,gs->dungeon.grid_dim);
        r_push_quad(.pos = pos, .col = ceil_color, .scale = scale, .rot = floor_rot);
      } else {
        // TODO: Can we just use backface culling for this?
        r_push_quad(.pos = pos, .sprite = sprite, .rot = floor_rot);
      }
    }

    Rangei perimeter_snippet = os_heat_distribute(visible_tile_list.num_perimeter);
    for each_in_range (tile, perimeter, perimeter_snippet) {
      Vec2 p0 = d_grid_to_world(&gs->dungeon, tile->grid_pos);
      Quat rot;
      Quat ceil_rot;
      Vec3 ceil_pos = v3(p0.x, ceil_height+0.002f, p0.y);
      if (tile->lateral) {
        rot = qi();
        ceil_rot = rot;
        ceil_pos.y += 0.001f;
        if (tile->requires_ceil_adjustment) {
          ceil_rot = axis_angle(v3(0,1,0), M_PI32);
          ceil_pos.x += gs->dungeon.grid_dim;
        }
      } else {
        rot = forward_wall_rot;
        ceil_rot = rot;
        if (tile->requires_ceil_adjustment) {
          ceil_rot = qinv(rot);
          ceil_pos.z -= gs->dungeon.grid_dim;
        }
      }
      for (u64 i = 0; i < wall_height; ++i) {
        f32 y = i * gs->dungeon.grid_dim;
        Vec3 world_pos = v3(p0.x, y, p0.y);
        r_push_quad(.pos = world_pos, .sprite = tile->sprite, .rot = rot);
      }
      r_push_quad(.pos = ceil_pos, .sprite = spr_ceil, .rot = qmul(ceil_rot, floor_rot));
    }

    os_heat_sync();

    if (runner_id() == 0) {
      r_draw_entity(&gs->player);
    }

    r_update_transform(VP);
    r_present(true);
  }
}